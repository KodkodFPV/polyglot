#include <catch2/catch_test_macros.hpp>

#include <polyglot/parser.hpp>
#include <polyglot/codec/binary_integer_le.hpp>
#include <polyglot/codec/binary_float_le.hpp>

#include <cstdint>
#include <array>
#include <expected>


namespace field = polyglot::schema::field;


using U8 = polyglot::codec::bin::IntegerLE<std::uint8_t>;
using F32 = polyglot::codec::bin::FloatLE<float>;


struct CodecWithoutWidthCheck
{
  std::expected<std::uint8_t, int> decode(field::ByteView bytes) const noexcept
  {
    return bytes[0];
  }
};


std::uint8_t checksum8(field::ByteView bytes)
{
  std::uint8_t result = 0;
  for (auto byte : bytes)
    result = static_cast<std::uint8_t>(result + byte);
  return result;
}


TEST_CASE("schema configuration")
{
  using Valid = polyglot::schema::Frame<field::Custom<1, 1, U8>, field::Data<field::Fixed<0>>>;
  polyglot::Parser valid{Valid{{}, {}}};
  CHECK(valid.ok());
  CHECK(!valid.error());

  using BadLength = polyglot::schema::Frame<field::Length<1, 1, 2, U8>, field::Data<>>;
  polyglot::Parser bad_length{BadLength{{}, {}}};
  CHECK(!bad_length.ok());
  CHECK(bad_length.error() == polyglot::err::ID::UnsupportedCodecWidth);
  {
    auto result = bad_length.process({});
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidConfiguration);
  }

  using BadChecksum = polyglot::schema::Frame<field::Data<field::Fixed<1>>, field::Checksum<0, 0, 2, std::uint8_t, U8, checksum8>>;
  polyglot::Parser bad_checksum{BadChecksum{{}, {}}};
  CHECK(!bad_checksum.ok());
  CHECK(bad_checksum.error() == polyglot::err::ID::UnsupportedCodecWidth);

  using BadCustom = polyglot::schema::Frame<field::Custom<1, 3, F32>, field::Data<field::Fixed<1>>>;
  polyglot::Parser bad_custom{BadCustom{{}, {}}};
  CHECK(!bad_custom.ok());
  CHECK(bad_custom.error() == polyglot::err::ID::UnsupportedCodecWidth);

  using ValidFloat = polyglot::schema::Frame<field::Custom<1, 4, F32>, field::Data<field::Fixed<0>>>;
  polyglot::Parser valid_float{ValidFloat{{}, {}}};
  CHECK(valid_float.ok());

  using UnrestrictedCodec = polyglot::schema::Frame<field::Custom<2, 3, CodecWithoutWidthCheck>, field::Data<field::Fixed<0>>>;
  polyglot::Parser unrestricted{UnrestrictedCodec{{}, {}}};
  CHECK(unrestricted.ok());
  const std::array<std::uint8_t, 3> unrestricted_bytes{0x41, 0x42, 0x43};
  CHECK(unrestricted.process(unrestricted_bytes) == unrestricted_bytes.size());

}

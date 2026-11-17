#include <catch2/catch_test_macros.hpp>

#include <polyglot/parser.hpp>
#include <polyglot/codec/binary_integer_le.hpp>

#include <cstdint>
#include <array>
#include <span>


namespace field = polyglot::schema::field;


using U8 = polyglot::codec::bin::IntegerLE<std::uint8_t>;


std::uint8_t checksum8(field::ByteView bytes)
{
  std::uint8_t result = 0;
  for (auto byte : bytes)
    result = static_cast<std::uint8_t>(result + byte);
  return result;
}


TEST_CASE("parser checksum")
{
  using AfterData = polyglot::schema::Frame<field::Data<field::Fixed<2>>, field::Checksum<0, 0, 1, std::uint8_t, U8, checksum8>>;
  polyglot::Parser after_data{AfterData{{}, {}}};
  const std::array<std::uint8_t, 3> valid{0x41, 0x42, 0x83};
  CHECK(after_data.process(valid) == valid.size());
  const std::array<std::uint8_t, 3> corrupt{0x41, 0x42, 0x82};
  {
    auto result = after_data.process(corrupt);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidChecksum);
  }
  {
    auto result = after_data.process(std::span{valid}.first(2));
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::FrameNotReady);
  }

  using BeforeData = polyglot::schema::Frame<field::Checksum<1, 1, 1, std::uint8_t, U8, checksum8>, field::Data<field::Fixed<2>>>;
  polyglot::Parser before_data{BeforeData{{}, {}}};
  const std::array<std::uint8_t, 3> before_valid{0x83, 0x41, 0x42};
  CHECK(before_data.process(before_valid) == before_valid.size());
  const std::array<std::uint8_t, 3> before_corrupt{0x82, 0x41, 0x42};
  {
    auto result = before_data.process(before_corrupt);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidChecksum);
  }

  const std::array<std::uint8_t, 1> start{0x02};
  using IncludesStart = polyglot::schema::Frame<field::Start<1>, field::Data<field::Fixed<2>>, field::Checksum<0, 1, 1, std::uint8_t, U8, checksum8>>;
  polyglot::Parser includes_start{IncludesStart{field::Start<1>{start}, {}, {}}};
  const std::array<std::uint8_t, 4> includes_start_valid{0x02, 0x41, 0x42, 0x85};
  CHECK(includes_start.process(includes_start_valid) == includes_start_valid.size());

}

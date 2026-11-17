#include <catch2/catch_test_macros.hpp>

#include <polyglot/parser.hpp>
#include <polyglot/codec/binary_integer_le.hpp>

#include <cstdint>
#include <array>


namespace field = polyglot::schema::field;

using U8 = polyglot::codec::bin::IntegerLE<std::uint8_t>;


std::uint8_t checksum8(field::ByteView bytes)
{
  std::uint8_t value = 0;
  for (auto byte : bytes)
    value = static_cast<std::uint8_t>(value + byte);
  return value;
}


TEST_CASE("parser fixed data length ranges")
{
  // The range spans four bytes, but Fixed<2> requires Length to be two.
  using InteriorRange = polyglot::schema::Frame<field::Length<1, 3, 1, U8>, field::Custom<7, 1, U8>, field::Data<field::Fixed<2>>, field::Checksum<2, 2, 1, std::uint8_t, U8, checksum8>>;
  polyglot::Parser interior{InteriorRange{{}, {}, {}, {}}};
  const std::array<std::uint8_t, 5> interior_valid{2, 0x55, 0x41, 0x42, 0x83};
  CHECK(interior.process(interior_valid) == interior_valid.size());
  for (std::uint8_t declared : {std::uint8_t{0}, std::uint8_t{3}, std::uint8_t{4}, std::uint8_t{5}, std::uint8_t{255}})
  {
    const std::array<std::uint8_t, 5> frame{declared, 0x55, 0x41, 0x42, 0x83};
    auto result = interior.process(frame);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidFrameLength);
  }

  // The range spans five bytes, but Fixed<1> requires Length to be one.
  const std::array<std::uint8_t, 1> start{0x02};
  const std::array<std::uint8_t, 1> stop{0x03};
  using WholeFrameRange = polyglot::schema::Frame<field::Start<1>, field::Length<0, 4, 1, U8>, field::Data<field::Fixed<1>>, field::Checksum<2, 2, 1, std::uint8_t, U8, checksum8>, field::Stop<1>>;
  polyglot::Parser whole{WholeFrameRange{field::Start<1>{start}, {}, {}, {}, field::Stop<1>{stop}}};
  const std::array<std::uint8_t, 5> whole_valid{0x02, 1, 0x41, 0x41, 0x03};
  CHECK(whole.process(whole_valid) == whole_valid.size());
  for (std::uint8_t declared : {std::uint8_t{0}, std::uint8_t{4}, std::uint8_t{5}})
  {
    const std::array<std::uint8_t, 5> frame{0x02, declared, 0x41, 0x41, 0x03};
    auto result = whole.process(frame);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidFrameLength);
  }

}

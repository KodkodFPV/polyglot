#include <catch2/catch_test_macros.hpp>

#include <polyglot/codec/binary_integer_be.hpp>
#include <polyglot/codec/binary_float_be.hpp>

#include <cstdint>
#include <cstddef>
#include <limits>
#include <bit>
#include <array>
#include <span>


using polyglot::codec::bin::IntegerBE;
using polyglot::codec::bin::FloatBE;
using polyglot::codec::Error;


template<typename T>
void big_endian_integer_round_trip()
{
  IntegerBE<T> codec;
  for (T value : {std::numeric_limits<T>::min(), T{0}, T{1}, std::numeric_limits<T>::max()})
  {
    std::array<std::uint8_t, sizeof(T)> bytes{};
    REQUIRE_FALSE(codec.encode(value, bytes).has_value());
    CHECK(codec.decode(bytes) == value);
  }
}


TEST_CASE("big endian integer codec")
{
  big_endian_integer_round_trip<std::uint8_t>();
  big_endian_integer_round_trip<std::int8_t>();
  big_endian_integer_round_trip<std::uint16_t>();
  big_endian_integer_round_trip<std::int16_t>();
  big_endian_integer_round_trip<std::uint32_t>();
  big_endian_integer_round_trip<std::int32_t>();
  big_endian_integer_round_trip<std::uint64_t>();
  big_endian_integer_round_trip<std::int64_t>();

  IntegerBE<std::uint32_t> unsigned32;
  std::array<std::uint8_t, 4> bytes{};
  REQUIRE_FALSE(unsigned32.encode(0x12345678, bytes).has_value());
  CHECK((bytes == std::array<std::uint8_t, 4>{0x12, 0x34, 0x56, 0x78}));
  CHECK(unsigned32.decode(bytes) == 0x12345678u);

  IntegerBE<std::int64_t> signed64;
  const std::array<std::uint8_t, 2> negative{0x80, 0x00};
  CHECK(signed64.decode(negative) == -32768);
  std::array<std::uint8_t, 1> narrow{0xA5};
  REQUIRE_FALSE(signed64.encode(-128, narrow).has_value());
  CHECK(narrow[0] == 0x80);
  CHECK(signed64.decode(narrow) == -128);
  CHECK_FALSE(signed64.encode(127, narrow).has_value());
  CHECK(signed64.encode(128, narrow) == Error::ValueOutOfRange);
  CHECK(signed64.encode(-129, narrow) == Error::ValueOutOfRange);
  CHECK(narrow[0] == 0x7F);
  CHECK(signed64.decode({}).error() == Error::InvalidSize);
  CHECK(signed64.encode(1, std::span<std::uint8_t>{}) == Error::InvalidSize);

  IntegerBE<std::uint64_t> unsigned64;
  for (std::size_t width = 1; width <= 8; ++width)
  {
    std::array<std::uint8_t, 8> wire{};
    auto slice = std::span{wire}.first(width);
    const auto max = width == 8 ? std::numeric_limits<std::uint64_t>::max() : (std::uint64_t{1} << (width * 8)) - 1;
    REQUIRE_FALSE(unsigned64.encode(max, slice).has_value());
    CHECK(unsigned64.decode(slice) == max);
    if (width < 8)
      CHECK(unsigned64.encode(max + 1, slice) == Error::ValueOutOfRange);
  }
}


template<typename T, typename Bits>
void big_endian_float_round_trip(Bits pattern)
{
  FloatBE<T> codec;
  const T value = std::bit_cast<T>(pattern);
  std::array<std::uint8_t, sizeof(T)> bytes{};
  REQUIRE_FALSE(codec.encode(value, bytes).has_value());
  const auto decoded = codec.decode(bytes);
  REQUIRE(decoded);
  CHECK(std::bit_cast<Bits>(*decoded) == pattern);
}


TEST_CASE("big endian float codec")
{
  static_assert(FloatBE<float>::accepts_size(4));
  static_assert(!FloatBE<float>::accepts_size(3));
  static_assert(FloatBE<double>::accepts_size(8));

  FloatBE<float> float_codec;
  std::array<std::uint8_t, 4> bytes{};
  REQUIRE_FALSE(float_codec.encode(1.0f, bytes).has_value());
  CHECK((bytes == std::array<std::uint8_t, 4>{0x3F, 0x80, 0x00, 0x00}));
  CHECK(float_codec.decode(bytes) == 1.0f);
  big_endian_float_round_trip<float>(std::uint32_t{0x80000000});
  big_endian_float_round_trip<float>(std::uint32_t{0x7F800000});
  big_endian_float_round_trip<float>(std::uint32_t{0x7FC12345});
  big_endian_float_round_trip<double>(std::uint64_t{0x7FF8123456789ABC});

  std::array<std::uint8_t, 3> short_bytes{0xA5, 0xA5, 0xA5};
  CHECK(float_codec.decode(short_bytes).error() == Error::InvalidSize);
  CHECK(float_codec.encode(1.0f, short_bytes) == Error::InvalidSize);
  CHECK((short_bytes == std::array<std::uint8_t, 3>{0xA5, 0xA5, 0xA5}));
}

#include <catch2/catch_test_macros.hpp>

#include <polyglot/codec/binary_integer_le.hpp>

#include <cstdint>
#include <cstddef>
#include <limits>
#include <array>
#include <span>


using polyglot::codec::bin::IntegerLE;
using polyglot::codec::Error;


template<typename T>
void full_width_round_trip()
{
  IntegerLE<T> codec;
  CHECK(!codec.accepts_size(0));
  CHECK(codec.accepts_size(1));
  CHECK(codec.accepts_size(sizeof(T)));
  CHECK(!codec.accepts_size(sizeof(T) + 1));

  for (T value : {std::numeric_limits<T>::min(), T{0}, T{1}, std::numeric_limits<T>::max()})
  {
    std::array<std::uint8_t, sizeof(T)> bytes{};
    CHECK_FALSE(codec.encode(value, bytes).has_value());
    auto decoded = codec.decode(bytes);
    REQUIRE(decoded.has_value());
    CHECK(*decoded == value);
  }
}


TEST_CASE("little endian integer codec")
{
  full_width_round_trip<std::uint8_t>();
  full_width_round_trip<std::int8_t>();
  full_width_round_trip<std::uint16_t>();
  full_width_round_trip<std::int16_t>();
  full_width_round_trip<std::uint32_t>();
  full_width_round_trip<std::int32_t>();
  full_width_round_trip<std::uint64_t>();
  full_width_round_trip<std::int64_t>();

  IntegerLE<std::uint32_t> unsigned32;
  std::array<std::uint8_t, 4> bytes{};
  CHECK_FALSE(unsigned32.encode(0x12345678, bytes).has_value());
  CHECK((bytes == std::array<std::uint8_t, 4>{0x78, 0x56, 0x34, 0x12}));
  CHECK(unsigned32.decode(bytes) == 0x12345678u);

  std::array<std::uint8_t, 1> narrow{0xA5};
  CHECK(unsigned32.decode(narrow) == 0xA5u);
  CHECK_FALSE(unsigned32.encode(255, narrow).has_value());
  CHECK(narrow[0] == 0xFF);
  {
    auto result = unsigned32.encode(256, narrow);
    CHECK(result == Error::ValueOutOfRange);
  }
  CHECK(narrow[0] == 0xFF);

  IntegerLE<std::int64_t> signed64;
  std::array<std::uint8_t, 1> signed_byte{0x80};
  CHECK(signed64.decode(signed_byte) == -128);
  signed_byte[0] = 0xFF;
  CHECK(signed64.decode(signed_byte) == -1);
  CHECK_FALSE(signed64.encode(-128, signed_byte).has_value());
  CHECK(signed_byte[0] == 0x80);
  CHECK_FALSE(signed64.encode(127, signed_byte).has_value());
  CHECK(signed_byte[0] == 0x7F);
  {
    auto result = signed64.encode(-129, signed_byte);
    CHECK(result == Error::ValueOutOfRange);
  }
  {
    auto result = signed64.encode(128, signed_byte);
    CHECK(result == Error::ValueOutOfRange);
  }
  CHECK(signed_byte[0] == 0x7F);

  const std::array<std::uint8_t, 2> negative_two_bytes{0x00, 0x80};
  CHECK(signed64.decode(negative_two_bytes) == -32768);

  IntegerLE<std::uint64_t> unsigned64;
  for (std::size_t width = 1; width <= 8; ++width)
  {
    std::array<std::uint8_t, 8> wire{};
    auto bytes_for_width = std::span{wire}.first(width);
    const auto unsigned_max = width == 8 ? std::numeric_limits<std::uint64_t>::max() : (std::uint64_t{1} << (width * 8)) - 1;
    CHECK_FALSE(unsigned64.encode(unsigned_max, bytes_for_width).has_value());
    CHECK(unsigned64.decode(bytes_for_width) == unsigned_max);
    if (width < 8)
    {
      auto overflow = unsigned64.encode(unsigned_max + 1, bytes_for_width);
      CHECK(overflow == Error::ValueOutOfRange);
    }

    const auto signed_limit = std::uint64_t{1} << (width * 8 - 1);
    const auto signed_min = width == 8 ? std::numeric_limits<std::int64_t>::min() : -static_cast<std::int64_t>(signed_limit);
    const auto signed_max = width == 8 ? std::numeric_limits<std::int64_t>::max() : static_cast<std::int64_t>(signed_limit - 1);
    CHECK_FALSE(signed64.encode(signed_min, bytes_for_width).has_value());
    CHECK(signed64.decode(bytes_for_width) == signed_min);
    CHECK_FALSE(signed64.encode(signed_max, bytes_for_width).has_value());
    CHECK(signed64.decode(bytes_for_width) == signed_max);
    if (width < 8)
    {
      auto underflow = signed64.encode(signed_min - 1, bytes_for_width);
      CHECK(underflow == Error::ValueOutOfRange);
      auto overflow = signed64.encode(signed_max + 1, bytes_for_width);
      CHECK(overflow == Error::ValueOutOfRange);
    }
  }

  std::array<std::uint8_t, 9> oversized{};
  {
    auto result = unsigned32.decode(std::span<const std::uint8_t>{});
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == Error::InvalidSize);
  }
  {
    auto result = unsigned32.decode(oversized);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == Error::InvalidSize);
  }
  {
    auto result = unsigned32.encode(1, std::span<std::uint8_t>{});
    CHECK(result == Error::InvalidSize);
  }
  {
    auto result = unsigned32.encode(1, oversized);
    CHECK(result == Error::InvalidSize);
  }

}

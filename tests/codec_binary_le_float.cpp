#include <catch2/catch_test_macros.hpp>

#include <polyglot/codec/binary_float_le.hpp>

#include <cstdint>
#include <bit>
#include <array>
#include <span>


using polyglot::codec::bin::FloatLE;
using polyglot::codec::Error;


template<typename T, typename Bits>
void preserves_bits(Bits pattern)
{
  FloatLE<T> codec;
  const T value = std::bit_cast<T>(pattern);
  std::array<std::uint8_t, sizeof(T)> bytes{};
  CHECK_FALSE(codec.encode(value, bytes).has_value());
  auto decoded = codec.decode(bytes);
  REQUIRE(decoded.has_value());
  CHECK(std::bit_cast<Bits>(*decoded) == pattern);
}


TEST_CASE("little endian float codec")
{
  static_assert(FloatLE<float>::accepts_size(4));
  static_assert(!FloatLE<float>::accepts_size(8));
  static_assert(FloatLE<double>::accepts_size(8));
  static_assert(!FloatLE<double>::accepts_size(4));

  FloatLE<float> float_codec;
  std::array<std::uint8_t, 4> float_bytes{};
  CHECK_FALSE(float_codec.encode(1.0f, float_bytes).has_value());
  CHECK((float_bytes == std::array<std::uint8_t, 4>{0x00, 0x00, 0x80, 0x3F}));
  CHECK(float_codec.decode(float_bytes) == 1.0f);

  FloatLE<double> double_codec;
  std::array<std::uint8_t, 8> double_bytes{};
  CHECK_FALSE(double_codec.encode(1.0, double_bytes).has_value());
  CHECK((double_bytes == std::array<std::uint8_t, 8>{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F}));
  CHECK(double_codec.decode(double_bytes) == 1.0);

  preserves_bits<float>(std::uint32_t{0x80000000});
  preserves_bits<float>(std::uint32_t{0x7F800000});
  preserves_bits<float>(std::uint32_t{0xFF800000});
  preserves_bits<float>(std::uint32_t{0x7FC12345});
  preserves_bits<double>(std::uint64_t{0x8000000000000000});
  preserves_bits<double>(std::uint64_t{0x7FF0000000000000});
  preserves_bits<double>(std::uint64_t{0xFFF0000000000000});
  preserves_bits<double>(std::uint64_t{0x7FF8123456789ABC});

  std::array<std::uint8_t, 3> short_bytes{0xA5, 0xA5, 0xA5};
  {
    auto result = float_codec.decode(std::span<const std::uint8_t>{});
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == Error::InvalidSize);
  }
  {
    auto result = float_codec.decode(short_bytes);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == Error::InvalidSize);
  }
  {
    auto result = float_codec.encode(1.0f, short_bytes);
    CHECK(result == Error::InvalidSize);
  }
  CHECK((short_bytes == std::array<std::uint8_t, 3>{0xA5, 0xA5, 0xA5}));
  std::array<std::uint8_t, 5> long_float_bytes{};
  {
    auto result = float_codec.decode(long_float_bytes);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == Error::InvalidSize);
  }
  {
    auto result = float_codec.encode(1.0f, long_float_bytes);
    CHECK(result == Error::InvalidSize);
  }
  {
    auto result = double_codec.decode(float_bytes);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == Error::InvalidSize);
  }
  {
    auto result = double_codec.encode(1.0, float_bytes);
    CHECK(result == Error::InvalidSize);
  }
  std::array<std::uint8_t, 9> long_double_bytes{};
  {
    auto result = double_codec.decode(long_double_bytes);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == Error::InvalidSize);
  }
  {
    auto result = double_codec.encode(1.0, long_double_bytes);
    CHECK(result == Error::InvalidSize);
  }

}

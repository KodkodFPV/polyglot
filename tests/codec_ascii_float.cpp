#include <catch2/catch_test_macros.hpp>

#include <polyglot/codec/ascii_float.hpp>

#include <cstdint>
#include <limits>
#include <cmath>
#include <charconv>
#include <bit>
#include <array>
#include <span>
#include <string_view>


using polyglot::codec::ascii::Float;
using polyglot::codec::Error;


std::span<const std::uint8_t> ascii_float_bytes(std::string_view text)
{
  return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}


template<std::size_t N>
std::string_view ascii_float_text(const std::array<std::uint8_t, N>& bytes)
{
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}


TEST_CASE("ASCII float places text first and zero pads the field")
{
  Float<float, std::chars_format::fixed, 1> codec;
  std::array<std::uint8_t, 8> bytes{};
  REQUIRE_FALSE(codec.encode(-12.5f, bytes).has_value());
  CHECK((bytes == std::array<std::uint8_t, 8>{'-', '1', '2', '.', '5', 0, 0, 0}));
  CHECK(codec.decode(bytes) == -12.5f);

  std::array<std::uint8_t, 5> exact{};
  REQUIRE_FALSE(codec.encode(-12.5f, exact).has_value());
  CHECK(ascii_float_text(exact).compare("-12.5") == 0);
  CHECK(codec.decode(exact) == -12.5f);

  std::array<std::uint8_t, 4> narrow{};
  CHECK(codec.encode(-12.5f, narrow) == Error::ValueOutOfRange);
  CHECK(codec.encode(1.0f, {}) == Error::InvalidSize);
  CHECK(codec.decode({}) == std::unexpected(Error::InvalidFormat));

  std::array<std::uint8_t, 7> guarded{'L', '?', '?', '?', '?', '?', 'R'};
  REQUIRE_FALSE(codec.encode(-12.5f, std::span{guarded}.subspan(1, 5)).has_value());
  CHECK(guarded.front() == 'L');
  CHECK(guarded.back() == 'R');
}


TEST_CASE("ASCII float writing format and precision are compile-time parameters")
{
  Float<float, std::chars_format::fixed, 2> fixed;
  std::array<std::uint8_t, 8> bytes{};
  REQUIRE_FALSE(fixed.encode(12.3456f, bytes).has_value());
  CHECK((bytes == std::array<std::uint8_t, 8>{'1', '2', '.', '3', '5', 0, 0, 0}));
  CHECK(fixed.decode(bytes) == 12.35f);

  std::array<std::uint8_t, 5> exact{};
  REQUIRE_FALSE(fixed.encode(12.0f, exact).has_value());
  CHECK(ascii_float_text(exact).compare("12.00") == 0);
  CHECK(fixed.encode(12.0f, std::span{exact}.first(4)) == Error::ValueOutOfRange);

  Float<float, std::chars_format::fixed, 0> no_fraction;
  std::array<std::uint8_t, 3> integer{};
  REQUIRE_FALSE(no_fraction.encode(12.0f, integer).has_value());
  CHECK((integer == std::array<std::uint8_t, 3>{'1', '2', 0}));

  Float<double, std::chars_format::fixed> double_codec;
  std::array<std::uint8_t, 12> double_bytes{};
  REQUIRE_FALSE(double_codec.encode(-12.5, double_bytes).has_value());
  CHECK((double_bytes == std::array<std::uint8_t, 12>{'-', '1', '2', '.', '5', '0', '0', '0', '0', '0', 0, 0}));
  CHECK(double_codec.decode(double_bytes) == -12.5);

  std::array<std::uint8_t, 4> small{};
  REQUIRE_FALSE((Float<double, std::chars_format::fixed, 2>{}.encode(0.000123, small).has_value()));
  CHECK((small == std::array<std::uint8_t, 4>{'0', '.', '0', '0'}));

  Float<double, std::chars_format::fixed, 25> precise;
  std::array<std::uint8_t, 32> precise_bytes{};
  REQUIRE_FALSE(precise.encode(0.1, precise_bytes).has_value());
  CHECK(precise.decode(precise_bytes) == 0.1);

  Float<float, std::chars_format::scientific, 2> scientific;
  REQUIRE_FALSE(scientific.encode(12.3456f, bytes).has_value());
  CHECK(ascii_float_text(bytes).compare("1.23e+01") == 0);
  CHECK(scientific.decode(bytes) == 12.3f);

  Float<float, std::chars_format::general, 3> general;
  REQUIRE_FALSE(general.encode(12.3456f, bytes).has_value());
  CHECK((bytes == std::array<std::uint8_t, 8>{'1', '2', '.', '3', 0, 0, 0, 0}));

  Float<double> default_codec;
  REQUIRE_FALSE(default_codec.encode(12.3456789, bytes).has_value());
  CHECK((bytes == std::array<std::uint8_t, 8>{'1', '2', '.', '3', '4', '5', '7', 0}));
}


TEST_CASE("ASCII float reader follows the selected format and accepts special values")
{
  Float<double, std::chars_format::fixed, 2> fixed;
  CHECK(fixed.decode(ascii_float_bytes("12")) == 12.0);
  CHECK(fixed.decode(ascii_float_bytes("12.")) == 12.0);
  CHECK(fixed.decode(ascii_float_bytes(".5")) == 0.5);
  CHECK(fixed.decode(ascii_float_bytes("-0.25")) == -0.25);
  CHECK(fixed.decode(ascii_float_bytes("1e3")) == std::unexpected(Error::InvalidCharacter));
  CHECK(fixed.decode(ascii_float_bytes("1.8p+1")) == std::unexpected(Error::InvalidCharacter));
  CHECK(fixed.decode(ascii_float_bytes("0x1.8p+1")) == std::unexpected(Error::InvalidCharacter));
  const auto negative_zero = fixed.decode(ascii_float_bytes("-0"));
  REQUIRE(negative_zero);
  CHECK(std::signbit(*negative_zero));

  Float<double, std::chars_format::scientific, 2> scientific;
  CHECK(scientific.decode(ascii_float_bytes("1e3")) == 1000.0);
  CHECK(scientific.decode(ascii_float_bytes("1E+3")) == 1000.0);
  CHECK(scientific.decode(ascii_float_bytes("12")) == std::unexpected(Error::InvalidCharacter));
  CHECK(scientific.decode(ascii_float_bytes("1.8p+1")) == std::unexpected(Error::InvalidCharacter));
  std::array<std::uint8_t, 5> padded{'1', 'e', '3', 0, 0};
  CHECK(scientific.decode(padded) == 1000.0);

  Float<double, std::chars_format::general, 2> general;
  CHECK(general.decode(ascii_float_bytes("12")) == 12.0);
  CHECK(general.decode(ascii_float_bytes("1e3")) == 1000.0);
  CHECK(general.decode(ascii_float_bytes("1.8p+1")) == std::unexpected(Error::InvalidCharacter));
  CHECK(general.decode(ascii_float_bytes("0x10")) == std::unexpected(Error::InvalidCharacter));

  const auto infinity = fixed.decode(ascii_float_bytes("inf"));
  REQUIRE(infinity);
  CHECK(std::isinf(*infinity));
  const auto negative_infinity = scientific.decode(ascii_float_bytes("-INFINITY"));
  REQUIRE(negative_infinity);
  CHECK(std::isinf(*negative_infinity));
  CHECK(std::signbit(*negative_infinity));
  const auto nan = general.decode(ascii_float_bytes("nan"));
  REQUIRE(nan);
  CHECK(std::isnan(*nan));
  CHECK(fixed.decode(ascii_float_bytes("1.23456789012345678901234567890123456789")).has_value());
}


TEST_CASE("ASCII float reader rejects malformed and out-of-range input")
{
  Float<float> codec;
  CHECK(codec.decode(std::array<std::uint8_t, 3>{0, 0, 0}) == std::unexpected(Error::InvalidFormat));
  CHECK(codec.decode(ascii_float_bytes("-")) == std::unexpected(Error::InvalidFormat));
  CHECK(codec.decode(ascii_float_bytes("0x")) == std::unexpected(Error::InvalidCharacter));
  CHECK(codec.decode(std::array<std::uint8_t, 4>{'1', 0, '2', 0}) == std::unexpected(Error::InvalidCharacter));
  CHECK(codec.decode(ascii_float_bytes("+1")) == std::unexpected(Error::InvalidCharacter));
  CHECK(codec.decode(ascii_float_bytes("12,5")) == std::unexpected(Error::InvalidCharacter));
  CHECK(codec.decode(ascii_float_bytes("1e")) == std::unexpected(Error::InvalidCharacter));
  CHECK(codec.decode(ascii_float_bytes("1p")) == std::unexpected(Error::InvalidCharacter));
  CHECK(codec.decode(ascii_float_bytes("0x1p")) == std::unexpected(Error::InvalidCharacter));
  CHECK(codec.decode(ascii_float_bytes("1e3x")) == std::unexpected(Error::InvalidCharacter));
  CHECK(codec.decode(ascii_float_bytes("1e1000")) == std::unexpected(Error::ValueOutOfRange));
  CHECK(codec.decode(ascii_float_bytes("1e-1000")) == std::unexpected(Error::ValueOutOfRange));
}


TEST_CASE("ASCII float writes and reads infinity and NaN")
{
  Float<float, std::chars_format::fixed, 2> codec;
  std::array<std::uint8_t, 8> bytes{};
  REQUIRE_FALSE(codec.encode(std::numeric_limits<float>::infinity(), bytes).has_value());
  const auto infinity = codec.decode(bytes);
  REQUIRE(infinity);
  CHECK(std::isinf(*infinity));
  REQUIRE_FALSE(codec.encode(-std::numeric_limits<float>::infinity(), bytes).has_value());
  const auto negative_infinity = codec.decode(bytes);
  REQUIRE(negative_infinity);
  CHECK(std::signbit(*negative_infinity));
  REQUIRE_FALSE(codec.encode(std::numeric_limits<float>::quiet_NaN(), bytes).has_value());
  const auto nan = codec.decode(bytes);
  REQUIRE(nan);
  CHECK(std::isnan(*nan));
}


TEST_CASE("ASCII float can round trip values when enough fractional digits are configured")
{
  Float<double, std::chars_format::fixed, 17> double_codec;
  for (double value : {0.1, -0.0})
  {
    std::array<std::uint8_t, 32> bytes{};
    REQUIRE_FALSE(double_codec.encode(value, bytes).has_value());
    const auto decoded = double_codec.decode(bytes);
    REQUIRE(decoded);
    CHECK(std::bit_cast<std::uint64_t>(*decoded) == std::bit_cast<std::uint64_t>(value));
  }

  Float<double, std::chars_format::fixed, 324> subnormal;
  std::array<std::uint8_t, 400> bytes{};
  REQUIRE_FALSE(subnormal.encode(std::numeric_limits<double>::denorm_min(), bytes).has_value());
  const auto decoded_subnormal = subnormal.decode(bytes);
  REQUIRE(decoded_subnormal);
  CHECK(std::bit_cast<std::uint64_t>(*decoded_subnormal) == std::bit_cast<std::uint64_t>(std::numeric_limits<double>::denorm_min()));

  Float<double, std::chars_format::fixed, 0> maximum;
  REQUIRE_FALSE(maximum.encode(std::numeric_limits<double>::max(), bytes).has_value());
  const auto decoded_maximum = maximum.decode(bytes);
  REQUIRE(decoded_maximum);
  CHECK(std::bit_cast<std::uint64_t>(*decoded_maximum) == std::bit_cast<std::uint64_t>(std::numeric_limits<double>::max()));

  Float<float, std::chars_format::fixed, 9> float_codec;
  std::array<std::uint8_t, 16> float_bytes{};
  REQUIRE_FALSE(float_codec.encode(0.1f, float_bytes).has_value());
  const auto decoded_float = float_codec.decode(float_bytes);
  REQUIRE(decoded_float);
  CHECK(std::bit_cast<std::uint32_t>(*decoded_float) == std::bit_cast<std::uint32_t>(0.1f));

  Float<double, std::chars_format::general, 17> precise_general;
  std::array<std::uint8_t, 32> general_bytes{};
  REQUIRE_FALSE(precise_general.encode(std::numeric_limits<double>::denorm_min(), general_bytes).has_value());
  const auto decoded_general = precise_general.decode(general_bytes);
  REQUIRE(decoded_general);
  CHECK(std::bit_cast<std::uint64_t>(*decoded_general) == std::bit_cast<std::uint64_t>(std::numeric_limits<double>::denorm_min()));
}

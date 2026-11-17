#include <catch2/catch_test_macros.hpp>

#include <polyglot/codec/ascii_decimal.hpp>
#include <polyglot/codec/ascii_hexadecimal.hpp>
#include <polyglot/codec/ascii_binary.hpp>

#include <cstdint>
#include <limits>
#include <algorithm>
#include <array>
#include <expected>
#include <span>
#include <string_view>


using polyglot::codec::ascii::Decimal;
using polyglot::codec::ascii::Hexadecimal;
using polyglot::codec::ascii::Binary;
namespace hex = polyglot::codec::ascii::hex;
namespace bin = polyglot::codec::ascii::bin;
using polyglot::codec::Error;


std::span<const std::uint8_t> ascii_bytes(std::string_view text)
{
  return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}


template<std::size_t N>
std::string_view ascii_text(const std::array<std::uint8_t, N>& bytes)
{
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}


TEST_CASE("ASCII decimal integer codec")
{
  Decimal<std::int64_t> signed_codec;
  std::array<std::uint8_t, 3> bytes{};
  REQUIRE_FALSE(signed_codec.encode(-42, bytes).has_value());
  CHECK(ascii_text(bytes).compare("-42") == 0);
  CHECK(signed_codec.decode(bytes) == -42);
  CHECK(signed_codec.decode(ascii_bytes("00042")) == 42);
  CHECK(signed_codec.decode(ascii_bytes("-9223372036854775808")) == std::numeric_limits<std::int64_t>::min());
  CHECK(signed_codec.decode(ascii_bytes("9223372036854775807")) == std::numeric_limits<std::int64_t>::max());
  CHECK(signed_codec.decode(ascii_bytes("-9223372036854775809")) == std::unexpected(Error::ValueOutOfRange));
  CHECK(signed_codec.decode(ascii_bytes("9223372036854775808")) == std::unexpected(Error::ValueOutOfRange));
  CHECK(signed_codec.decode(ascii_bytes("+42")) == std::unexpected(Error::InvalidCharacter));
  CHECK(signed_codec.decode(ascii_bytes("4 2")) == std::unexpected(Error::InvalidCharacter));
  CHECK(signed_codec.decode(ascii_bytes("42x")) == std::unexpected(Error::InvalidCharacter));
  CHECK(signed_codec.decode(ascii_bytes("-")) == std::unexpected(Error::InvalidFormat));

  Decimal<std::uint64_t> unsigned_codec;
  CHECK(unsigned_codec.decode(ascii_bytes("18446744073709551615")) == std::numeric_limits<std::uint64_t>::max());
  CHECK(unsigned_codec.decode(ascii_bytes("18446744073709551616")) == std::unexpected(Error::ValueOutOfRange));
  CHECK(unsigned_codec.decode(ascii_bytes("-1")) == std::unexpected(Error::InvalidCharacter));
  std::array<std::uint8_t, 1> narrow{'Z'};
  CHECK(signed_codec.encode(-42, narrow) == Error::ValueOutOfRange);
  std::array<std::uint8_t, 5> oversized{};
  REQUIRE_FALSE(signed_codec.encode(-42, oversized).has_value());
  CHECK((oversized == std::array<std::uint8_t, 5>{'-', '4', '2', 0, 0}));
  CHECK(signed_codec.decode(oversized) == -42);
}


TEST_CASE("ASCII decimal rejects misplaced zero bytes")
{
  Decimal<std::int16_t> codec;
  CHECK(codec.decode(std::array<std::uint8_t, 4>{0, 0, 0, 0}) == std::unexpected(Error::InvalidFormat));
  CHECK(codec.decode(std::array<std::uint8_t, 4>{0, 0, '4', '2'}) == std::unexpected(Error::InvalidFormat));
  CHECK(codec.decode(std::array<std::uint8_t, 4>{'4', 0, '2', 0}) == std::unexpected(Error::InvalidCharacter));
}


TEST_CASE("ASCII hexadecimal integer codec and prefix policies")
{
  Hexadecimal<std::int16_t, hex::Prefix::Lower> lower;
  std::array<std::uint8_t, 7> bytes{'?', '?', '?', '?', '?', '?', '?'};
  REQUIRE_FALSE(lower.encode(-42, bytes).has_value());
  CHECK(ascii_text(bytes).compare("-0x2A??") == 0);
  CHECK(lower.decode(ascii_bytes("-0x2A")) == -42);
  CHECK(lower.decode(ascii_bytes("0xA")) == 10);
  CHECK(lower.decode(ascii_bytes("-0xA")) == -10);
  CHECK(lower.decode(ascii_bytes("-0x002a")) == std::unexpected(Error::InvalidCharacter));
  CHECK(lower.decode(ascii_bytes("-0X002a")) == std::unexpected(Error::InvalidFormat));
  CHECK(lower.decode(ascii_bytes("-002A")) == std::unexpected(Error::InvalidFormat));
  std::array<std::uint8_t, 7> positive{'?', '?', '?', '?', '?', '?', '?'};
  REQUIRE_FALSE(lower.encode(42, positive).has_value());
  CHECK(ascii_text(positive).compare("0x2A???") == 0);
  CHECK(lower.decode(std::span<const std::uint8_t>{positive}.first(4)) == 42);

  Hexadecimal<std::int16_t, hex::Prefix::Upper> upper;
  bytes.fill('?');
  REQUIRE_FALSE(upper.encode(-42, bytes).has_value());
  CHECK(ascii_text(bytes).compare("-0X2A??") == 0);
  CHECK(upper.decode(ascii_bytes("-0X002a")) == std::unexpected(Error::InvalidCharacter));
  CHECK(upper.decode(ascii_bytes("0XA")) == 10);
  CHECK(upper.decode(ascii_bytes("-0x002a")) == std::unexpected(Error::InvalidFormat));

  Hexadecimal<std::int16_t, hex::Prefix::Lower, hex::LetterCase::Mixed> mixed;
  bytes.fill('?');
  REQUIRE_FALSE(mixed.encode(-42, bytes).has_value());
  CHECK(ascii_text(bytes).compare("-0x2a??") == 0);
  CHECK(mixed.decode(ascii_bytes("-0x002a")) == -42);
  CHECK(mixed.decode(ascii_bytes("-0x002A")) == -42);
  CHECK(mixed.decode(ascii_bytes("-0X002A")) == std::unexpected(Error::InvalidFormat));

  Hexadecimal<std::int8_t> plain;
  CHECK(plain.decode(ascii_bytes("-80")) == std::numeric_limits<std::int8_t>::min());
  CHECK(plain.decode(ascii_bytes("80")) == std::unexpected(Error::ValueOutOfRange));
  CHECK(plain.decode(ascii_bytes("0x7F")) == std::unexpected(Error::InvalidCharacter));
  CHECK(plain.decode(ascii_bytes("GG")) == std::unexpected(Error::InvalidCharacter));
  CHECK(plain.decode(ascii_bytes("2AG")) == std::unexpected(Error::InvalidCharacter));
  CHECK(plain.decode(ascii_bytes("+2A")) == std::unexpected(Error::InvalidCharacter));
  CHECK(plain.decode(ascii_bytes("-")) == std::unexpected(Error::InvalidFormat));
  std::array<std::uint8_t, 2> narrow{'Z', 'Z'};
  CHECK(lower.encode(1, narrow) == Error::ValueOutOfRange);

  Hexadecimal<std::int64_t> signed64;
  std::array<std::uint8_t, 17> minimum{};
  REQUIRE_FALSE(signed64.encode(std::numeric_limits<std::int64_t>::min(), minimum).has_value());
  CHECK(ascii_text(minimum).compare("-8000000000000000") == 0);
  CHECK(signed64.decode(minimum) == std::numeric_limits<std::int64_t>::min());
  CHECK(signed64.decode(ascii_bytes("8000000000000000")) == std::unexpected(Error::ValueOutOfRange));
  Hexadecimal<std::int64_t, hex::Prefix::Lower> signed64_prefixed;
  CHECK(signed64_prefixed.decode(ascii_bytes("-0x8000000000000000")) == std::numeric_limits<std::int64_t>::min());
  CHECK(signed64_prefixed.decode(ascii_bytes("0x8000000000000000")) == std::unexpected(Error::ValueOutOfRange));
  CHECK(signed64.decode(ascii_bytes("000000000000000000")) == 0);
}


TEST_CASE("ASCII hexadecimal prefix and letter case are independent")
{
  constexpr std::uint32_t value = 0xAABBCCDD;
  std::array<std::uint8_t, 10> prefixed{};
  std::array<std::uint8_t, 8> plain{};

  Hexadecimal<std::uint32_t, hex::Prefix::Lower, hex::LetterCase::Upper> lower_upper;
  REQUIRE_FALSE(lower_upper.encode(value, prefixed).has_value());
  CHECK(ascii_text(prefixed).compare("0xAABBCCDD") == 0);
  CHECK(lower_upper.decode(prefixed) == value);

  Hexadecimal<std::uint32_t, hex::Prefix::Upper, hex::LetterCase::Upper> upper_upper;
  REQUIRE_FALSE(upper_upper.encode(value, prefixed).has_value());
  CHECK(ascii_text(prefixed).compare("0XAABBCCDD") == 0);
  CHECK(upper_upper.decode(prefixed) == value);

  Hexadecimal<std::uint32_t, hex::Prefix::Lower, hex::LetterCase::Lower> lower_lower;
  REQUIRE_FALSE(lower_lower.encode(value, prefixed).has_value());
  CHECK(ascii_text(prefixed).compare("0xaabbccdd") == 0);
  CHECK(lower_lower.decode(prefixed) == value);
  CHECK(lower_lower.decode(ascii_bytes("0xAABBCCDD")) == std::unexpected(Error::InvalidCharacter));

  Hexadecimal<std::uint32_t, hex::Prefix::Upper, hex::LetterCase::Lower> upper_lower;
  REQUIRE_FALSE(upper_lower.encode(value, prefixed).has_value());
  CHECK(ascii_text(prefixed).compare("0Xaabbccdd") == 0);
  CHECK(upper_lower.decode(prefixed) == value);

  Hexadecimal<std::uint32_t, hex::Prefix::None, hex::LetterCase::Upper> none_upper;
  REQUIRE_FALSE(none_upper.encode(value, plain).has_value());
  CHECK(ascii_text(plain).compare("AABBCCDD") == 0);
  CHECK(none_upper.decode(plain) == value);

  Hexadecimal<std::uint32_t, hex::Prefix::None, hex::LetterCase::Lower> none_lower;
  REQUIRE_FALSE(none_lower.encode(value, plain).has_value());
  CHECK(ascii_text(plain).compare("aabbccdd") == 0);
  CHECK(none_lower.decode(plain) == value);

  Hexadecimal<std::uint32_t, hex::Prefix::Lower, hex::LetterCase::Mixed> mixed_lower_prefix;
  REQUIRE_FALSE(mixed_lower_prefix.encode(value, prefixed).has_value());
  CHECK(ascii_text(prefixed).compare("0xaabbccdd") == 0);
  CHECK(mixed_lower_prefix.decode(ascii_bytes("0xAABBCCDD")) == value);
  CHECK(mixed_lower_prefix.decode(ascii_bytes("0XAABBCCDD")) == std::unexpected(Error::InvalidFormat));
  CHECK(mixed_lower_prefix.decode(plain) == std::unexpected(Error::InvalidFormat));
  CHECK(mixed_lower_prefix.encode(value, plain) == Error::ValueOutOfRange);

  std::array<std::uint8_t, 4> unpadded{'?', '?', '?', '?'};
  REQUIRE_FALSE(Hexadecimal<std::uint8_t>{}.encode(0x2a, unpadded).has_value());
  CHECK((unpadded == std::array<std::uint8_t, 4>{'2', 'A', '?', '?'}));

  using Padded = Hexadecimal<std::int16_t, hex::Prefix::None, hex::LetterCase::Upper, true>;
  std::array<std::uint8_t, 5> padded{};
  REQUIRE_FALSE(Padded{}.encode(-42, padded).has_value());
  CHECK((padded == std::array<std::uint8_t, 5>{'-', '0', '0', '2', 'A'}));
  CHECK(Padded{}.decode(padded) == -42);

  using PrefixedPadded = Hexadecimal<std::int8_t, hex::Prefix::Lower, hex::LetterCase::Upper, true>;
  std::array<std::uint8_t, 5> prefixed_padded{};
  REQUIRE_FALSE(PrefixedPadded{}.encode(-1, prefixed_padded).has_value());
  CHECK((prefixed_padded == std::array<std::uint8_t, 5>{'-', '0', 'x', '0', '1'}));
  CHECK(PrefixedPadded{}.decode(prefixed_padded) == -1);
}


template<typename Codec, typename T>
void check_hex_value(T value, std::string_view expected)
{
  Codec codec;
  std::array<std::uint8_t, 32> bytes{};
  bytes.fill('?');

  REQUIRE_FALSE(codec.encode(value, std::span{bytes}.first(expected.size())).has_value());
  CHECK(std::string_view(reinterpret_cast<const char*>(bytes.data()), expected.size()).compare(expected) == 0);
  CHECK(codec.decode(ascii_bytes(expected)) == value);
  CHECK(std::all_of(bytes.begin() + expected.size(), bytes.end(), [](std::uint8_t byte) { return byte == '?'; }));

  bytes.fill('?');
  CHECK(codec.encode(value, std::span{bytes}.first(expected.size() - 1)) == Error::ValueOutOfRange);
  CHECK(std::all_of(bytes.begin(), bytes.end(), [](std::uint8_t byte) { return byte == '?'; }));

  REQUIRE_FALSE(codec.encode(value, bytes).has_value());
  CHECK(std::string_view(reinterpret_cast<const char*>(bytes.data()), expected.size()).compare(expected) == 0);
  CHECK(std::all_of(bytes.begin() + expected.size(), bytes.end(), [](std::uint8_t byte) { return byte == '?'; }));
}


TEST_CASE("ASCII hexadecimal values and capacities at integer limits")
{
  check_hex_value<Hexadecimal<std::int8_t>>(std::numeric_limits<std::int8_t>::min(), "-80");
  check_hex_value<Hexadecimal<std::int8_t>>(std::numeric_limits<std::int8_t>::max(), "7F");
  check_hex_value<Hexadecimal<std::int16_t>>(std::numeric_limits<std::int16_t>::min(), "-8000");
  check_hex_value<Hexadecimal<std::int16_t>>(std::numeric_limits<std::int16_t>::max(), "7FFF");
  check_hex_value<Hexadecimal<std::int32_t>>(std::numeric_limits<std::int32_t>::min(), "-80000000");
  check_hex_value<Hexadecimal<std::int32_t>>(std::numeric_limits<std::int32_t>::max(), "7FFFFFFF");
  check_hex_value<Hexadecimal<std::int64_t, hex::Prefix::Lower>>(std::numeric_limits<std::int64_t>::min(), "-0x8000000000000000");
  check_hex_value<Hexadecimal<std::int64_t, hex::Prefix::Lower>>(std::numeric_limits<std::int64_t>::max(), "0x7FFFFFFFFFFFFFFF");
  check_hex_value<Hexadecimal<std::uint8_t>>(std::numeric_limits<std::uint8_t>::max(), "FF");
  check_hex_value<Hexadecimal<std::uint16_t>>(std::numeric_limits<std::uint16_t>::max(), "FFFF");
  check_hex_value<Hexadecimal<std::uint32_t>>(std::numeric_limits<std::uint32_t>::max(), "FFFFFFFF");
  check_hex_value<Hexadecimal<std::uint64_t, hex::Prefix::Upper>>(std::numeric_limits<std::uint64_t>::max(), "0XFFFFFFFFFFFFFFFF");
  check_hex_value<Hexadecimal<std::uint8_t>>(std::uint8_t{0}, "0");
  check_hex_value<Hexadecimal<std::uint8_t, hex::Prefix::Lower>>(std::uint8_t{0}, "0x0");
}


TEST_CASE("ASCII hexadecimal format, range and remaining policy combinations")
{
  Hexadecimal<std::int8_t, hex::Prefix::Lower> signed_lower;
  CHECK(signed_lower.decode(ascii_bytes("")) == std::unexpected(Error::InvalidFormat));
  CHECK(signed_lower.decode(ascii_bytes("-")) == std::unexpected(Error::InvalidFormat));
  CHECK(signed_lower.decode(ascii_bytes("0x")) == std::unexpected(Error::InvalidFormat));
  CHECK(signed_lower.decode(ascii_bytes("-0x")) == std::unexpected(Error::InvalidFormat));
  CHECK(signed_lower.decode(ascii_bytes("-0X1")) == std::unexpected(Error::InvalidFormat));
  CHECK(signed_lower.decode(ascii_bytes("0x80")) == std::unexpected(Error::ValueOutOfRange));
  CHECK(signed_lower.decode(ascii_bytes("-0x81")) == std::unexpected(Error::ValueOutOfRange));
  CHECK(signed_lower.decode(ascii_bytes("0x1g")) == std::unexpected(Error::InvalidCharacter));
  CHECK(signed_lower.decode(ascii_bytes(std::string_view{"0x1\0", 4})) == std::unexpected(Error::InvalidCharacter));

  Hexadecimal<std::int64_t, hex::Prefix::Lower> signed64;
  CHECK(signed64.decode(ascii_bytes("0x8000000000000000")) == std::unexpected(Error::ValueOutOfRange));
  CHECK(signed64.decode(ascii_bytes("-0x8000000000000001")) == std::unexpected(Error::ValueOutOfRange));

  Hexadecimal<std::uint8_t, hex::Prefix::Upper> unsigned_upper;
  CHECK(unsigned_upper.decode(ascii_bytes("-0X1")) == std::unexpected(Error::InvalidFormat));
  CHECK(unsigned_upper.decode(ascii_bytes("0X100")) == std::unexpected(Error::ValueOutOfRange));

  Hexadecimal<std::uint64_t, hex::Prefix::Upper> unsigned64;
  CHECK(unsigned64.decode(ascii_bytes("0X10000000000000000")) == std::unexpected(Error::ValueOutOfRange));

  Hexadecimal<std::uint8_t, hex::Prefix::Upper, hex::LetterCase::Mixed> upper_mixed;
  check_hex_value<decltype(upper_mixed)>(std::uint8_t{0xab}, "0Xab");
  CHECK(upper_mixed.decode(ascii_bytes("0XAB")) == 0xab);
  CHECK(upper_mixed.decode(ascii_bytes("0xAB")) == std::unexpected(Error::InvalidFormat));

  Hexadecimal<std::uint8_t, hex::Prefix::None, hex::LetterCase::Mixed> none_mixed;
  check_hex_value<decltype(none_mixed)>(std::uint8_t{0xab}, "ab");
  CHECK(none_mixed.decode(ascii_bytes("aB")) == 0xab);
  CHECK(none_mixed.decode(ascii_bytes("0xab")) == std::unexpected(Error::InvalidCharacter));
}


TEST_CASE("ASCII hexadecimal zero padding covers digits only")
{
  check_hex_value<Hexadecimal<std::int8_t, hex::Prefix::Upper, hex::LetterCase::Lower, true>>(std::int8_t{-10}, "-0X0a");
  check_hex_value<Hexadecimal<std::uint16_t, hex::Prefix::Lower, hex::LetterCase::Upper, true>>(std::uint16_t{10}, "0x000A");
  check_hex_value<Hexadecimal<std::int64_t, hex::Prefix::None, hex::LetterCase::Upper, true>>(std::numeric_limits<std::int64_t>::min(), "-8000000000000000");
}


TEST_CASE("ASCII hexadecimal prefixed input has a 19-byte limit")
{
  Hexadecimal<std::int64_t, hex::Prefix::Lower> codec;
  CHECK(codec.decode(ascii_bytes("0x00000000000000001")) == 1);
  CHECK(codec.decode(ascii_bytes("-0x0000000000000001")) == -1);
  CHECK(codec.decode(ascii_bytes("0x000000000000000001")) == std::unexpected(Error::InvalidFormat));
  CHECK(codec.decode(ascii_bytes("-0x00000000000000001")) == std::unexpected(Error::InvalidFormat));
}


TEST_CASE("ASCII binary integer codec and prefix policies")
{
  Binary<std::int16_t, bin::Prefix::Lower> lower;
  std::array<std::uint8_t, 18> bytes{};
  REQUIRE_FALSE(lower.encode(-42, bytes).has_value());
  CHECK(ascii_text(bytes).compare("0b1111111111010110") == 0);
  CHECK(lower.decode(bytes) == -42);
  CHECK(lower.decode(ascii_bytes("0B1111111111010110")) == std::unexpected(Error::InvalidFormat));
  CHECK(lower.decode(ascii_bytes("001111111111010110")) == std::unexpected(Error::InvalidFormat));
  CHECK(lower.decode(ascii_bytes("-0b111111111101011")) == std::unexpected(Error::InvalidFormat));

  Binary<std::int16_t, bin::Prefix::Upper> upper;
  REQUIRE_FALSE(upper.encode(-42, bytes).has_value());
  CHECK(ascii_text(bytes).compare("0B1111111111010110") == 0);
  CHECK(upper.decode(bytes) == -42);
  CHECK(upper.decode(ascii_bytes("0b1111111111010110")) == std::unexpected(Error::InvalidFormat));

  Binary<std::int8_t> plain;
  CHECK(plain.decode(ascii_bytes("01010010")) == 82);
  CHECK(plain.decode(ascii_bytes("10101101")) == -83);
  CHECK(plain.decode(ascii_bytes("10000000")) == std::numeric_limits<std::int8_t>::min());
  CHECK(plain.decode(ascii_bytes("11111111")) == -1);
  CHECK(plain.decode(ascii_bytes("-0000001")) == std::unexpected(Error::InvalidCharacter));
  CHECK(plain.decode(ascii_bytes("0b101010")) == std::unexpected(Error::InvalidCharacter));
  CHECK(plain.decode(ascii_bytes("00000012")) == std::unexpected(Error::InvalidCharacter));
  CHECK(plain.decode(ascii_bytes("0000001x")) == std::unexpected(Error::InvalidCharacter));
  std::array<std::uint8_t, 8> plain_bytes{};
  REQUIRE_FALSE(plain.encode(-1, plain_bytes).has_value());
  CHECK(ascii_text(plain_bytes).compare("11111111") == 0);
  plain_bytes.fill('X');
  REQUIRE_FALSE(plain.encode(1, plain_bytes).has_value());
  CHECK(ascii_text(plain_bytes).compare("00000001") == 0);
  std::array<std::uint8_t, 7> narrow{};
  CHECK(plain.encode(2, narrow) == Error::InvalidSize);

  Binary<std::uint8_t> unsigned8;
  CHECK(unsigned8.decode(ascii_bytes("10101101")) == 173);

  Binary<std::int64_t> signed64;
  std::array<std::uint8_t, 64> minimum{};
  REQUIRE_FALSE(signed64.encode(std::numeric_limits<std::int64_t>::min(), minimum).has_value());
  CHECK(signed64.decode(minimum) == std::numeric_limits<std::int64_t>::min());

  Binary<std::uint64_t> unsigned64;
  std::array<std::uint8_t, 64> maximum{};
  REQUIRE_FALSE(unsigned64.encode(std::numeric_limits<std::uint64_t>::max(), maximum).has_value());
  CHECK(unsigned64.decode(maximum) == std::numeric_limits<std::uint64_t>::max());
}


TEST_CASE("ASCII integer encoders stay within the caller span")
{
  std::array<std::uint8_t, 5> guarded{'L', 'Z', 'Z', 'Z', 'R'};
  std::span<std::uint8_t> output = std::span{guarded}.subspan(1, 3);

  CHECK(Decimal<std::int16_t>{}.encode(-1234, output) == Error::ValueOutOfRange);
  CHECK(output.size() == 3);
  CHECK(guarded.front() == 'L');
  CHECK(guarded.back() == 'R');

  CHECK((Hexadecimal<std::int16_t, hex::Prefix::Lower>{}.encode(-1, output) == Error::ValueOutOfRange));
  CHECK(guarded.front() == 'L');
  CHECK(guarded.back() == 'R');

  CHECK((Binary<std::int16_t, bin::Prefix::Upper>{}.encode(-1, output) == Error::InvalidSize));
  CHECK(guarded.front() == 'L');
  CHECK(guarded.back() == 'R');

  std::array<std::uint8_t, 4> exact{'L', 'Z', 'Z', 'R'};
  REQUIRE_FALSE(Decimal<std::int16_t>{}.encode(42, std::span{exact}.subspan(1, 2)).has_value());
  CHECK(ascii_text(exact).compare("L42R") == 0);
}

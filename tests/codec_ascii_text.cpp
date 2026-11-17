#include <catch2/catch_test_macros.hpp>

#include <polyglot/codec/ascii_text.hpp>
#include <polyglot/parser.hpp>

#include <cstdint>
#include <array>
#include <span>
#include <string_view>


using polyglot::codec::ascii::Text;
using polyglot::codec::Error;


TEST_CASE("ASCII text encodes exact and padded fields")
{
  Text codec;
  static_assert(Text::accepts_size(1));
  static_assert(!Text::accepts_size(0));

  std::array<std::uint8_t, 2> exact{'?', '?'};
  REQUIRE_FALSE(codec.encode("OK", exact).has_value());
  CHECK((exact == std::array<std::uint8_t, 2>{'O', 'K'}));
  auto exact_text = codec.decode(exact);
  REQUIRE(exact_text.has_value());
  CHECK(exact_text->compare("OK") == 0);

  std::array<std::uint8_t, 4> padded{'?', '?', '?', '?'};
  REQUIRE_FALSE(codec.encode("OK", padded).has_value());
  CHECK((padded == std::array<std::uint8_t, 4>{'O', 'K', 0, 0}));
  auto decoded = codec.decode(padded);
  REQUIRE(decoded.has_value());
  CHECK(decoded->compare("OK") == 0);
  CHECK(decoded->data() == reinterpret_cast<const char*>(padded.data()));

  REQUIRE_FALSE(codec.encode("", padded).has_value());
  CHECK((padded == std::array<std::uint8_t, 4>{0, 0, 0, 0}));
  auto empty = codec.decode(padded);
  REQUIRE(empty.has_value());
  CHECK(empty->empty());
}


TEST_CASE("ASCII text accepts the full seven-bit character set except embedded zero")
{
  Text codec;
  const std::array<char, 4> characters{1, '\t', '\n', static_cast<char>(0x7F)};
  const std::string_view text{characters.data(), characters.size()};
  std::array<std::uint8_t, 4> bytes{};

  REQUIRE_FALSE(codec.encode(text, bytes).has_value());
  CHECK((bytes == std::array<std::uint8_t, 4>{1, '\t', '\n', 0x7F}));
  auto decoded = codec.decode(bytes);
  REQUIRE(decoded.has_value());
  CHECK(decoded->compare(text) == 0);
}


TEST_CASE("ASCII text rejects malformed padding and non-ASCII bytes")
{
  Text codec;
  CHECK(codec.decode(std::array<std::uint8_t, 4>{'A', 0, 'B', 0}) == std::unexpected(Error::InvalidFormat));
  CHECK(codec.decode(std::array<std::uint8_t, 4>{'A', 0, 0, 'B'}) == std::unexpected(Error::InvalidFormat));
  CHECK(codec.decode(std::array<std::uint8_t, 2>{0x80, 0}) == std::unexpected(Error::InvalidCharacter));
  CHECK(codec.decode(std::array<std::uint8_t, 2>{'A', 0xFF}) == std::unexpected(Error::InvalidCharacter));
  CHECK(codec.decode({}) == std::unexpected(Error::InvalidSize));

  std::array<std::uint8_t, 3> output{'?', '?', '?'};
  const std::array<char, 1> non_ascii{static_cast<char>(0x80)};
  CHECK(codec.encode(std::string_view{non_ascii.data(), non_ascii.size()}, output) == Error::InvalidCharacter);
  CHECK(codec.encode(std::string_view{"A\0B", 3}, output) == Error::InvalidCharacter);
  CHECK(codec.encode("ABCD", output) == Error::ValueOutOfRange);
  CHECK((output == std::array<std::uint8_t, 3>{'?', '?', '?'}));
  CHECK(codec.encode("", {}) == Error::InvalidSize);
}


TEST_CASE("ASCII text writes only within the supplied span")
{
  Text codec;
  std::array<std::uint8_t, 5> guarded{'L', '?', '?', '?', 'R'};
  REQUIRE_FALSE(codec.encode("A", std::span{guarded}.subspan(1, 3)).has_value());
  CHECK((guarded == std::array<std::uint8_t, 5>{'L', 'A', 0, 0, 'R'}));
}


TEST_CASE("parser validates an ASCII text custom field")
{
  namespace field = polyglot::schema::field;
  using Frame = polyglot::schema::Frame<field::Custom<9, 4, Text>, field::Data<field::Fixed<0>>>;
  polyglot::Parser parser{Frame{{}, {}}};

  const std::array<std::uint8_t, 4> valid{'O', 'K', 0, 0};
  CHECK(parser.ok());
  CHECK(parser.process(valid) == valid.size());

  const std::array<std::uint8_t, 4> invalid{'O', 0, 'K', 0};
  CHECK(parser.process(invalid) == std::unexpected(polyglot::err::ID::InvalidCustomField));
}

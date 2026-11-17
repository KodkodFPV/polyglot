#include <catch2/catch_test_macros.hpp>

#include <polyglot/parser.hpp>
#include <polyglot/codec/binary_integer_le.hpp>
#include <polyglot/codec/binary_integer_be.hpp>
#include <polyglot/codec/binary_float_le.hpp>
#include <polyglot/codec/binary_float_be.hpp>
#include <polyglot/codec/ascii_decimal.hpp>
#include <polyglot/codec/ascii_hexadecimal.hpp>
#include <polyglot/codec/ascii_binary.hpp>
#include <polyglot/codec/ascii_float.hpp>
#include <polyglot/codec/ascii_text.hpp>
#include <charconv>
#include <cstdint>
#include <array>
#include <span>


namespace field = polyglot::schema::field;


using U8 = polyglot::codec::bin::IntegerLE<std::uint8_t>;


std::uint8_t checksum8(field::ByteView bytes)
{
  std::uint8_t value = 0;
  for (auto byte : bytes)
    value = static_cast<std::uint8_t>(value + byte);
  return value;
}


TEST_CASE("parser sample frame compiles and runs")
{
  using Frame = polyglot::schema::Frame<field::Start<1>, field::Length<3, 3, 1, U8>, field::Custom<1, 1, U8>, field::Data<>, field::Checksum<3, 3, 1, std::uint8_t, U8, checksum8>, field::Stop<1>>;

  const std::array<std::uint8_t, 1> start{0x02};
  const std::array<std::uint8_t, 1> stop{0x03};
  const std::array<std::uint8_t, 7> bytes{0x02, 0x02, 0x05, 0x41, 0x42, 0x83, 0x03};

  polyglot::Parser parser{Frame{field::Start<1>{start}, {}, {}, field::Data<>{}, {}, field::Stop<1>{stop}}};
  CHECK(parser.process(bytes) == bytes.size());
}

TEST_CASE("all codec templates compile")
{
  std::array<std::uint8_t, 8> bytes{};
  std::span<std::uint8_t> two = std::span{bytes}.first(2);
  std::span<std::uint8_t> four = std::span{bytes}.first(4);
  std::span<std::uint8_t> output = bytes;
  CHECK_FALSE(polyglot::codec::bin::IntegerLE<std::uint16_t>{}.encode(1, two).has_value());
  CHECK_FALSE(polyglot::codec::bin::IntegerBE<std::uint16_t>{}.encode(1, two).has_value());
  CHECK_FALSE(polyglot::codec::bin::FloatLE<float>{}.encode(1.0f, four).has_value());
  CHECK_FALSE(polyglot::codec::bin::FloatBE<float>{}.encode(1.0f, four).has_value());
  CHECK_FALSE(polyglot::codec::ascii::Decimal<std::int16_t>{}.encode(-1, two).has_value());
  CHECK_FALSE((polyglot::codec::ascii::Hexadecimal<std::int16_t, polyglot::codec::ascii::hex::Prefix::Lower, polyglot::codec::ascii::hex::LetterCase::Mixed>{}.encode(-1, output).has_value()));
  CHECK_FALSE((polyglot::codec::ascii::Hexadecimal<std::uint16_t, polyglot::codec::ascii::hex::Prefix::Upper, polyglot::codec::ascii::hex::LetterCase::Lower>{}.encode(0xabcd, output).has_value()));
  const std::array<std::uint8_t, 5> signed_hex{'-', '0', 'x', '8', '0'};
  CHECK((polyglot::codec::ascii::Hexadecimal<std::int8_t, polyglot::codec::ascii::hex::Prefix::Lower>{}.decode(signed_hex) == -128));
  std::array<std::uint8_t, 18> binary_output{};
  CHECK_FALSE((polyglot::codec::ascii::Binary<std::int16_t, polyglot::codec::ascii::bin::Prefix::Lower>{}.encode(-1, binary_output).has_value()));
  const std::array<std::uint8_t, 8> binary_digits{'0', '0', '0', '0', '1', '0', '1', '0'};
  CHECK(polyglot::codec::ascii::Binary<std::uint8_t>{}.decode(binary_digits) == 10);
  static_assert(polyglot::codec::ascii::Binary<std::uint8_t>::accepts_size(8));
  static_assert(!polyglot::codec::ascii::Binary<std::uint8_t>::accepts_size(7));
  CHECK_FALSE(polyglot::codec::ascii::Float<float>{}.encode(1.5f, output).has_value());
  CHECK_FALSE((polyglot::codec::ascii::Float<double, std::chars_format::fixed>{}.encode(1.5, output).has_value()));
  CHECK((polyglot::codec::ascii::Float<float, std::chars_format::scientific>{}.decode(std::array<std::uint8_t, 4>{'1', 'e', '+', '0'}) == 1.0f));
}

TEST_CASE("parser reads ASCII decimal custom field")
{
  using Decimal = polyglot::codec::ascii::Decimal<std::uint16_t>;
  using Frame = polyglot::schema::Frame<field::Custom<7, 4, Decimal>, field::Data<field::Fixed<0>>>;
  const std::array<std::uint8_t, 4> bytes{'4', '2', 0, 0};

  polyglot::Parser parser{Frame{{}, {}}};
  CHECK(parser.process(bytes) == bytes.size());
}



TEST_CASE("parser reads fixed-width ASCII binary custom field")
{
  using Binary = polyglot::codec::ascii::Binary<std::uint8_t>;
  using Frame = polyglot::schema::Frame<field::Custom<8, 8, Binary>, field::Data<field::Fixed<0>>>;
  const std::array<std::uint8_t, 8> bytes{'0', '1', '0', '1', '0', '0', '1', '0'};

  polyglot::Parser parser{Frame{{}, {}}};
  CHECK(parser.ok());
  CHECK(parser.process(bytes) == bytes.size());
}

TEST_CASE("parser reads ASCII text custom field")
{
  using Text = polyglot::codec::ascii::Text;
  using Frame = polyglot::schema::Frame<field::Custom<9, 4, Text>, field::Data<field::Fixed<0>>>;
  const std::array<std::uint8_t, 4> bytes{'O', 'K', 0, 0};

  polyglot::Parser parser{Frame{{}, {}}};
  CHECK(parser.ok());
  CHECK(parser.process(bytes) == bytes.size());
}

#include <catch2/catch_test_macros.hpp>

#include <polyglot/parser.hpp>
#include <polyglot/codec/binary_integer_le.hpp>

#include <cstdint>
#include <cstddef>
#include <array>
#include <span>
#include <expected>


namespace field = polyglot::schema::field;


using U8 = polyglot::codec::bin::IntegerLE<std::uint8_t>;


enum class FieldId
{
  First,
  Second,
};

struct RejectFF
{
  static constexpr bool accepts_size(std::size_t size) noexcept { return size == 1; }

  std::expected<std::uint8_t, int> decode(field::ByteView bytes) const noexcept
  {
    if (bytes[0] == 0xFF)
      return std::unexpected(1);
    return bytes[0];
  }
};


TEST_CASE("parser custom fields")
{
  using Frame = polyglot::schema::Frame<field::Custom<FieldId::First, 1, RejectFF>, field::Data<field::Fixed<1>>, field::Custom<FieldId::Second, 1, RejectFF>>;
  polyglot::Parser parser{Frame{{}, {}, {}}};
  const std::array<std::uint8_t, 3> valid{0x10, 0x41, 0x20};
  CHECK(parser.process(valid) == valid.size());
  const std::array<std::uint8_t, 3> invalid_first{0xFF, 0x41, 0x20};
  {
    auto result = parser.process(invalid_first);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidCustomField);
  }
  const std::array<std::uint8_t, 3> invalid_second{0x10, 0x41, 0xFF};
  {
    auto result = parser.process(invalid_second);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidCustomField);
  }
  {
    auto result = parser.process(std::span{valid}.first(2));
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::FrameNotReady);
  }

}

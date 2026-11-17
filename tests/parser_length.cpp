#include <catch2/catch_test_macros.hpp>

#include <polyglot/parser.hpp>
#include <polyglot/codec/binary_integer_le.hpp>

#include <cstdint>
#include <array>
#include <span>


namespace field = polyglot::schema::field;


using U8 = polyglot::codec::bin::IntegerLE<std::uint8_t>;
using I8 = polyglot::codec::bin::IntegerLE<std::int8_t>;


TEST_CASE("parser length")
{
  using FixedOnly = polyglot::schema::Frame<field::Data<field::Fixed<3>>>;
  polyglot::Parser fixed_only{FixedOnly{{}}};
  const std::array<std::uint8_t, 4> with_trailing{0x41, 0x42, 0x43, 0x99};
  CHECK(fixed_only.process(with_trailing) == 3);
  {
    auto result = fixed_only.process(std::span{with_trailing}.first(2));
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::FrameNotReady);
  }

  using LengthWithGap = polyglot::schema::Frame<field::Length<1, 3, 1, U8>, field::Custom<7, 1, U8>, field::Data<>, field::Stop<1>>;
  const std::array<std::uint8_t, 1> stop{0x03};
  polyglot::Parser gap{LengthWithGap{{}, {}, {}, field::Stop<1>{stop}}};
  const std::array<std::uint8_t, 5> gap_valid{4, 0x55, 0x41, 0x42, 0x03};
  CHECK(gap.process(gap_valid) == gap_valid.size());
  const std::array<std::uint8_t, 3> gap_too_short{1, 0x55, 0x03};
  {
    auto result = gap.process(gap_too_short);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidFrameLength);
  }
  const std::array<std::uint8_t, 4> gap_incomplete{4, 0x55, 0x41, 0x42};
  {
    auto result = gap.process(gap_incomplete);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::FrameNotReady);
  }

  using SelfInclusive = polyglot::schema::Frame<field::Length<0, 1, 1, U8>, field::Data<>>;
  polyglot::Parser self_inclusive{SelfInclusive{{}, {}}};
  const std::array<std::uint8_t, 3> self_valid{3, 0x41, 0x42};
  CHECK(self_inclusive.process(self_valid) == self_valid.size());

  using ZeroLength = polyglot::schema::Frame<field::Length<1, 1, 1, U8>, field::Data<>>;
  polyglot::Parser zero_length{ZeroLength{{}, {}}};
  const std::array<std::uint8_t, 1> zero_payload{0};
  CHECK(zero_length.process(zero_payload) == zero_payload.size());

  using SignedLength = polyglot::schema::Frame<field::Length<1, 1, 1, I8>, field::Data<>>;
  polyglot::Parser signed_length{SignedLength{{}, {}}};
  const std::array<std::uint8_t, 1> negative_length{0xFF};
  {
    auto result = signed_length.process(negative_length);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidFrameLength);
  }

  // With Fixed data, the decoded Length must equal the configured Data width.
  using FixedWithLength = polyglot::schema::Frame<field::Length<1, 1, 1, U8>, field::Data<field::Fixed<2>>>;
  polyglot::Parser fixed_with_length{FixedWithLength{{}, {}}};
  const std::array<std::uint8_t, 3> correct_fixed_length{2, 0x41, 0x42};
  CHECK(fixed_with_length.process(correct_fixed_length) == correct_fixed_length.size());
  const std::array<std::uint8_t, 3> wrong_fixed_length{1, 0x41, 0x42};
  {
    auto result = fixed_with_length.process(wrong_fixed_length);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidFrameLength);
  }

  // The Fixed width wins even when the declared range excludes Data.
  using LengthOutsideData = polyglot::schema::Frame<field::Length<0, 0, 1, U8>, field::Data<field::Fixed<2>>>;
  polyglot::Parser outside_data{LengthOutsideData{{}, {}}};
  const std::array<std::uint8_t, 3> correct_outside_length{2, 0x41, 0x42};
  CHECK(outside_data.process(correct_outside_length) == correct_outside_length.size());
  const std::array<std::uint8_t, 3> wrong_outside_length{1, 0x41, 0x42};
  {
    auto result = outside_data.process(wrong_outside_length);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidFrameLength);
  }

  using EmptyFixed = polyglot::schema::Frame<field::Length<1, 1, 1, U8>, field::Data<field::Fixed<0>>>;
  polyglot::Parser empty_fixed{EmptyFixed{{}, {}}};
  const std::array<std::uint8_t, 1> correct_empty_length{0};
  CHECK(empty_fixed.process(correct_empty_length) == correct_empty_length.size());
  const std::array<std::uint8_t, 1> wrong_empty_length{1};
  {
    auto result = empty_fixed.process(wrong_empty_length);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidFrameLength);
  }

}

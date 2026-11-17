#include <catch2/catch_test_macros.hpp>

#include <polyglot/parser.hpp>

#include <cstdint>
#include <array>
#include <span>


namespace field = polyglot::schema::field;


TEST_CASE("parser markers")
{
  const std::array<std::uint8_t, 2> start{0xAA, 0x55};
  const std::array<std::uint8_t, 1> stop{0x03};
  const std::array<std::uint8_t, 2> delimiter{0x0D, 0x0A};
  using Marked = polyglot::schema::Frame<field::Start<2>, field::Data<field::Fixed<1>>, field::Stop<1>, field::Delimiter<2>>;
  polyglot::Parser marked{Marked{field::Start<2>{start}, {}, field::Stop<1>{stop}, field::Delimiter<2>{delimiter}}};

  const std::array<std::uint8_t, 6> valid{0xAA, 0x55, 0x41, 0x03, 0x0D, 0x0A};
  CHECK(marked.process(valid) == valid.size());
  const std::array<std::uint8_t, 6> bad_start{0xAA, 0x54, 0x41, 0x03, 0x0D, 0x0A};
  {
    auto result = marked.process(bad_start);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidStart);
  }
  const std::array<std::uint8_t, 6> bad_stop{0xAA, 0x55, 0x41, 0x04, 0x0D, 0x0A};
  {
    auto result = marked.process(bad_stop);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidStop);
  }
  const std::array<std::uint8_t, 6> bad_delimiter{0xAA, 0x55, 0x41, 0x03, 0x0D, 0x0B};
  {
    auto result = marked.process(bad_delimiter);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidDelimetr);
  }
  {
    auto result = marked.process(std::span{valid}.first(1));
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::FrameNotReady);
  }

  // The caller owns marker bytes; the parser observes changes to that storage.
  std::array<std::uint8_t, 1> source_start{0x7E};
  using StartOnly = polyglot::schema::Frame<field::Start<1>, field::Data<field::Fixed<0>>>;
  polyglot::Parser borrowed_start{StartOnly{field::Start<1>{source_start}, {}}};
  source_start[0] = 0x7F;
  const std::array<std::uint8_t, 1> updated_start{0x7F};
  const std::array<std::uint8_t, 1> previous_start{0x7E};
  CHECK(borrowed_start.process(updated_start) == updated_start.size());
  {
    auto result = borrowed_start.process(previous_start);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidStart);
  }

  std::array<std::uint8_t, 1> source_stop{0x03};
  using StopOnly = polyglot::schema::Frame<field::Data<field::Fixed<0>>, field::Stop<1>>;
  polyglot::Parser borrowed_stop{StopOnly{{}, field::Stop<1>{source_stop}}};
  source_stop[0] = 0x04;
  const std::array<std::uint8_t, 1> updated_stop{0x04};
  const std::array<std::uint8_t, 1> previous_stop{0x03};
  CHECK(borrowed_stop.process(updated_stop) == updated_stop.size());
  {
    auto result = borrowed_stop.process(previous_stop);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidStop);
  }

  std::array<std::uint8_t, 1> source_delimiter{0x0A};
  using DelimiterOnly = polyglot::schema::Frame<field::Data<field::Fixed<0>>, field::Delimiter<1>>;
  polyglot::Parser borrowed_delimiter{DelimiterOnly{{}, field::Delimiter<1>{source_delimiter}}};
  source_delimiter[0] = 0x0B;
  const std::array<std::uint8_t, 1> updated_delimiter{0x0B};
  const std::array<std::uint8_t, 1> previous_delimiter{0x0A};
  CHECK(borrowed_delimiter.process(updated_delimiter) == updated_delimiter.size());
  {
    auto result = borrowed_delimiter.process(previous_delimiter);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidDelimetr);
  }

}

#include <catch2/catch_test_macros.hpp>

#include <polyglot/parser.hpp>
#include <polyglot/codec/binary_integer_le.hpp>

#include <cstdint>
#include <cstddef>
#include <array>
#include <span>


namespace field = polyglot::schema::field;


using U8 = polyglot::codec::bin::IntegerLE<std::uint8_t>;


std::uint8_t checksum8(field::ByteView bytes)
{
  std::uint8_t result = 0;
  for (auto byte : bytes)
    result = static_cast<std::uint8_t>(result + byte);
  return result;
}


TEST_CASE("parser partial input")
{
  using Frame = polyglot::schema::Frame<field::Start<1>, field::Length<2, 2, 1, U8>, field::Data<>, field::Checksum<2, 2, 1, std::uint8_t, U8, checksum8>, field::Stop<1>>;
  const std::array<std::uint8_t, 1> start{0x02};
  const std::array<std::uint8_t, 1> stop{0x03};
  polyglot::Parser parser{Frame{field::Start<1>{start}, {}, {}, {}, field::Stop<1>{stop}}};
  const std::array<std::uint8_t, 6> first{0x02, 0x02, 0x41, 0x42, 0x83, 0x03};

  for (std::size_t size = 0; size < first.size(); ++size)
  {
    auto result = parser.process(std::span{first}.first(size));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == polyglot::err::ID::FrameNotReady);
  }
  CHECK(parser.process(first) == first.size());

  const std::array<std::uint8_t, 5> second{0x02, 0x01, 0x43, 0x43, 0x03};
  CHECK(parser.process(second) == second.size());

  const std::array<std::uint8_t, 11> combined{0x02, 0x02, 0x41, 0x42, 0x83, 0x03, 0x02, 0x01, 0x43, 0x43, 0x03};
  auto consumed = parser.process(combined);
  CHECK(consumed == first.size());
  if (consumed)
    CHECK(parser.process(std::span{combined}.subspan(*consumed)) == second.size());

  using MarkerBoundary = polyglot::schema::Frame<field::Data<>, field::Stop<1>>;
  polyglot::Parser marker_boundary{MarkerBoundary{{}, field::Stop<1>{stop}}};
  const std::array<std::uint8_t, 3> marker_frame{0x41, 0x42, 0x03};
  {
    auto result = marker_boundary.process(std::span{marker_frame}.first(2));
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::FrameNotReady);
  }
  CHECK(marker_boundary.process(marker_frame) == marker_frame.size());

  const std::array<std::uint8_t, 1> delimiter{0x0A};
  using TrailingFields = polyglot::schema::Frame<field::Data<>, field::Custom<1, 1, U8>, field::Stop<1>, field::Delimiter<1>>;
  polyglot::Parser trailing{TrailingFields{{}, {}, field::Stop<1>{stop}, field::Delimiter<1>{delimiter}}};
  const std::array<std::uint8_t, 5> trailing_frame{0x41, 0x42, 0x55, 0x03, 0x0A};
  CHECK(trailing.process(trailing_frame) == trailing_frame.size());
  const std::array<std::uint8_t, 5> trailing_bad_stop{0x41, 0x42, 0x55, 0x04, 0x0A};
  {
    auto result = trailing.process(trailing_bad_stop);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidStop);
  }
  {
    auto result = trailing.process(std::span{trailing_frame}.first(4));
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::FrameNotReady);
  }
  const std::array<std::uint8_t, 1> delimiter_too_early{0x0A};
  {
    auto result = trailing.process(delimiter_too_early);
    CHECK_FALSE(result.has_value());
    if (!result)
      CHECK(result.error() == polyglot::err::ID::InvalidFrameLength);
  }

}

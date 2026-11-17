#include <catch2/catch_test_macros.hpp>

#include <polyglot/codec/binary_integer_le.hpp>
#include <polyglot/schema/traits.hpp>

#include <cstdint>
#include <tuple>


namespace field = polyglot::schema::field;
namespace traits = polyglot::schema::traits;


using U8 = polyglot::codec::bin::IntegerLE<std::uint8_t>;

std::uint8_t checksum8(field::ByteView bytes)
{
  std::uint8_t result = 0;
  for (auto byte : bytes)
    result = static_cast<std::uint8_t>(result + byte);
  return result;
}

using Frame = polyglot::schema::Frame<field::Start<1>, field::Length<3, 3, 1, U8>, field::Custom<42, 1, U8>, field::Data<>, field::Checksum<3, 3, 1, std::uint8_t, U8, checksum8>, field::Stop<1>, field::Delimiter<1>>;


static_assert(traits::is_start<field::Start<1>>);
static_assert(traits::is_stop<field::Stop<1>>);
static_assert(traits::is_delimiter<field::Delimiter<1>>);
static_assert(traits::is_length_field<field::Length<3, 3, 1, U8>>);
static_assert(traits::is_data<field::Data<>>);
static_assert(traits::is_fixed_data<field::Data<field::Fixed<0>>>);
static_assert(!traits::is_fixed_data<field::Data<>>);
static_assert(traits::is_checksum<field::Checksum<3, 3, 1, std::uint8_t, U8, checksum8>>);
static_assert(traits::is_custom_id<42, field::Custom<42, 1, U8>>);
static_assert(!traits::is_custom_id<43, field::Custom<42, 1, U8>>);

static_assert(traits::position_start<Frame> == 0);
static_assert(traits::position_length<Frame> == 1);
static_assert(traits::position_custom<42, Frame> == 2);
static_assert(traits::position_data<Frame> == 3);
static_assert(traits::position_checksum<Frame> == 4);
static_assert(traits::position_stop<Frame> == 5);
static_assert(traits::position_delimiter<Frame> == 6);
static_assert(traits::position_custom<43, Frame> == traits::npos);
static_assert(traits::range<traits::position_length<Frame>, field::Start<1>, field::Length<3, 3, 1, U8>, field::Custom<42, 1, U8>, field::Data<>, field::Checksum<3, 3, 1, std::uint8_t, U8, checksum8>, field::Stop<1>, field::Delimiter<1>>().first == 3);
static_assert(traits::length_covers_data<Frame>);
static_assert(!traits::length_covers_data<polyglot::schema::Frame<field::Data<field::Fixed<1>>>>);


TEST_CASE("schema traits")
{
  Frame frame{{}, {}, {}, {}, {}, {}, {}};
  CHECK(&traits::field_start(frame) == &std::get<0>(frame.fields));
  CHECK(&traits::field_custom<42>(frame) == &std::get<2>(frame.fields));
  CHECK(&traits::field_data(frame) == &std::get<3>(frame.fields));
  CHECK(&traits::field_checksum(frame) == &std::get<4>(frame.fields));
  CHECK(&traits::field_delimiter(frame) == &std::get<6>(frame.fields));
}

#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>
#include <array>
#include <tuple>

#include <polyglot/schema/fields.hpp>
#include <polyglot/schema/traits.hpp>


namespace polyglot::schema::check
{
  namespace detail
  {
    template<typename A, typename B>
    inline constexpr bool same_custom_id = false;

    template<auto ID, std::size_t SizeA, std::size_t SizeB, typename CodecA, typename CodecB>
    inline constexpr bool same_custom_id<field::Custom<ID, SizeA, CodecA>, field::Custom<ID, SizeB, CodecB>> = true;

    template<typename Field, typename... Fields>
    inline constexpr std::size_t id_occurrences = (std::size_t{0} + ... + static_cast<std::size_t>(same_custom_id<Field, Fields>));

    template<typename... Fields>
    inline constexpr bool unique_custom_ids = ((id_occurrences<Fields, Fields...> <= 1) && ...);
  }

  template<typename... Fields>
  consteval void validate_structure(std::type_identity<Frame<Fields...>>)
  {
    constexpr std::size_t field_count = sizeof...(Fields);

    static_assert(((traits::is_start<Fields>        || traits::is_stop<Fields>  || traits::is_delimiter<Fields> ||
                    traits::is_length_field<Fields> || traits::is_data<Fields>  || traits::is_checksum<Fields>  ||
                    traits::is_custom<Fields>) && ...), "Frame contains an unsupported field");

    constexpr auto count_start     = (std::size_t{0} + ... + static_cast<std::size_t>(traits::is_start<Fields>));
    constexpr auto count_stop      = (std::size_t{0} + ... + static_cast<std::size_t>(traits::is_stop<Fields>));
    constexpr auto count_delimiter = (std::size_t{0} + ... + static_cast<std::size_t>(traits::is_delimiter<Fields>));
    constexpr auto count_data      = (std::size_t{0} + ... + static_cast<std::size_t>(traits::is_data<Fields>));
    constexpr auto count_length    = (std::size_t{0} + ... + static_cast<std::size_t>(traits::is_length_field<Fields>));
    constexpr auto count_checksum  = (std::size_t{0} + ... + static_cast<std::size_t>(traits::is_checksum<Fields>));

    static_assert(count_start <= 1,     "Frame can contain at most one Start field");
    static_assert(count_stop <= 1,      "Frame can contain at most one Stop field");
    static_assert(count_delimiter <= 1, "Frame can contain at most one Delimiter field");
    static_assert(count_data == 1,      "Frame must contain exactly one Data field");
    static_assert(count_length <= 1,    "Frame can contain at most one Length field");
    static_assert(count_checksum <= 1,  "Frame can contain at most one Checksum field");
    static_assert(detail::unique_custom_ids<Fields...>, "Custom fields IDs must be unique within a Frame");

    using FrameType = Frame<Fields...>;
    constexpr auto pos_start     = traits::position_start<FrameType>;
    constexpr auto pos_stop      = traits::position_stop<FrameType>;
    constexpr auto pos_delimiter = traits::position_delimiter<FrameType>;
    constexpr auto pos_data      = traits::position_data<FrameType>;
    constexpr auto pos_length    = traits::position_length<FrameType>;
    constexpr auto pos_checksum  = traits::position_checksum<FrameType>;

    static_assert(count_start == 0 || pos_start == 0, "Start must be first");
    static_assert(count_delimiter == 0 || pos_delimiter + 1 == field_count, "Delimiter must be last");
    static_assert(count_stop == 0 || pos_stop + 1 + count_delimiter == field_count, "Stop must end the frame");
    static_assert(count_length == 0 || pos_length < pos_data, "Length must precede Data");

    constexpr auto length_range = traits::range<pos_length, Fields...>();

    static_assert(count_length == 0 || length_range.first < field_count, "Length::from must refer to an existing zero-based field position");
    static_assert(count_length == 0 || length_range.second < field_count, "Length::to must refer to an existing zero-based field position");
    static_assert(count_length == 0 || length_range.first <= length_range.second, "Length::from must not follow Length::to");

    constexpr auto checksum_range = traits::range<pos_checksum, Fields...>();

    static_assert(count_checksum == 0 || checksum_range.first < field_count, "Checksum::from must refer to an existing zero-based field position");
    static_assert(count_checksum == 0 || checksum_range.second < field_count, "Checksum::to must refer to an existing zero-based field position");
    static_assert(count_checksum == 0 || checksum_range.first <= checksum_range.second, "Checksum::from must not follow Checksum::to");
    static_assert(count_checksum == 0 || !(checksum_range.first <= pos_checksum && pos_checksum <= checksum_range.second), "Checksum range must not include the Checksum field");

    constexpr bool fixed_data = (traits::is_fixed_data<Fields> || ...);
    constexpr bool length_covers_data = count_length == 1 && length_range.first <= pos_data && pos_data <= length_range.second;

    static_assert(fixed_data || length_covers_data || count_stop == 1 || count_delimiter == 1, "Data has no defined boundary");
  }
}

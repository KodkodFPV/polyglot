#pragma once

#include <cstddef>

#include <polyglot/schema/fields.hpp>
#include <polyglot/schema/frame.hpp>


namespace polyglot::schema::traits
{
  template<std::size_t Pos, typename... Fields>
  consteval auto range()
  {
    if constexpr (Pos == npos)
      return std::pair{std::size_t{0}, std::size_t{0}};
    else
    {
      using T = std::tuple_element_t<Pos, std::tuple<Fields...>>;
      return std::pair{T::from, T::to};
    }
  }

  template<typename T>
  inline constexpr bool length_covers_data = false;

  template<typename... Fields>
  inline constexpr bool length_covers_data<Frame<Fields...>> = []
  {
    constexpr auto length_pos = position_length<Frame<Fields...>>;
    constexpr auto data_pos = position_data<Frame<Fields...>>;

    if constexpr (length_pos == npos || data_pos == npos)
      return false;
    else
    {
      constexpr auto bounds = range<length_pos, Fields...>();
      return bounds.first <= data_pos && data_pos <= bounds.second;
    }
  }();
}

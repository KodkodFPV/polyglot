#pragma once

#include <cstddef>

#include <polyglot/schema/fields.hpp>

#include <polyglot/schema/traits/identity.hpp>


namespace polyglot::schema::traits
{
  template<typename... Fields>
  constexpr decltype(auto) field_start(const Frame<Fields...>& frame)
    requires ((is_start<Fields> || ...))
  {
    return std::get<position_start<Frame<Fields...>>>(frame.fields);
  }

  template<typename... Fields>
  constexpr decltype(auto) field_stop(const Frame<Fields...>& frame)
    requires ((is_stop<Fields> || ...))
  {
    return std::get<position_stop<Frame<Fields...>>>(frame.fields);
  }

  template<typename... Fields>
  constexpr decltype(auto) field_delimiter(const Frame<Fields...>& frame)
    requires ((is_delimiter<Fields> || ...))
  {
    return std::get<position_delimiter<Frame<Fields...>>>(frame.fields);
  }

  template<typename... Fields>
  constexpr decltype(auto) field_length(const Frame<Fields...>& frame)
    requires ((is_length_field<Fields> || ...))
  {
    return std::get<position_length<Frame<Fields...>>>(frame.fields);
  }

  template<typename... Fields>
  constexpr decltype(auto) field_data(const Frame<Fields...>& frame)
  {
    return std::get<position_data<Frame<Fields...>>>(frame.fields);
  }

  template<typename... Fields>
  constexpr decltype(auto) field_checksum(const Frame<Fields...>& frame)
    requires ((is_checksum<Fields> || ...))
  {
    return std::get<position_checksum<Frame<Fields...>>>(frame.fields);
  }

  template<auto ID, typename... Fields>
  constexpr decltype(auto) field_custom(const Frame<Fields...>& frame)
    requires ((is_custom_id<ID, Fields> || ...))
  {
    return std::get<position_custom<ID, Frame<Fields...>>>(frame.fields);
  }
}

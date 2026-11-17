#pragma once

#include <cstddef>
#include <array>

#include <polyglot/schema/fields.hpp>
#include <polyglot/schema/frame.hpp>


namespace polyglot::schema::traits
{
  inline constexpr std::size_t npos = static_cast<std::size_t>(-1);

  template<std::size_t N>
  consteval std::size_t position(const std::array<bool, N>& found)
  {
    for (std::size_t i = 0; i < found.size(); ++i)
      if (found[i])
        return i;

    return npos;
  }

  template<typename T>
  inline constexpr std::size_t position_start = npos;

  template<typename... Fields>
  inline constexpr std::size_t position_start<Frame<Fields...>> = position(std::array{is_start<Fields>...});

  template<typename T>
  inline constexpr std::size_t position_stop = npos;

  template<typename... Fields>
  inline constexpr std::size_t position_stop<Frame<Fields...>> = position(std::array{is_stop<Fields>...});

  template<typename T>
  inline constexpr std::size_t position_delimiter = npos;

  template<typename... Fields>
  inline constexpr std::size_t position_delimiter<Frame<Fields...>> = position(std::array{is_delimiter<Fields>...});

  template<typename T>
  inline constexpr std::size_t position_length = npos;

  template<typename... Fields>
  inline constexpr std::size_t position_length<Frame<Fields...>> = position(std::array{is_length_field<Fields>...});

  template<typename T>
  inline constexpr std::size_t position_data = npos;

  template<typename... Fields>
  inline constexpr std::size_t position_data<Frame<Fields...>> = position(std::array{is_data<Fields>...});

  template<typename T>
  inline constexpr std::size_t position_checksum = npos;

  template<typename... Fields>
  inline constexpr std::size_t position_checksum<Frame<Fields...>> = position(std::array{is_checksum<Fields>...});

  template<auto ID, typename T>
  inline constexpr std::size_t position_custom = npos;

  template<auto ID, typename... Fields>
  inline constexpr std::size_t position_custom<ID, Frame<Fields...>> = position(std::array{is_custom_id<ID, Fields>...});
}

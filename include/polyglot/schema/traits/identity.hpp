#pragma once

#include <cstddef>

#include <polyglot/schema/fields.hpp>


namespace polyglot::schema::traits
{
  template<typename T>
  inline constexpr bool is_start = false;

  template<std::size_t N>
  inline constexpr bool is_start<field::Start<N>> = true;

  template<typename T>
  inline constexpr bool is_stop = false;

  template<std::size_t N>
  inline constexpr bool is_stop<field::Stop<N>> = true;

  template<typename T>
  inline constexpr bool is_delimiter = false;

  template<std::size_t N>
  inline constexpr bool is_delimiter<field::Delimiter<N>> = true;

  template<typename T>
  inline constexpr bool is_length_field = false;

  template<std::size_t From, std::size_t To, std::size_t Size, typename Codec>
  inline constexpr bool is_length_field<field::Length<From, To, Size, Codec>> = true;

  template<typename T>
  inline constexpr bool is_data = false;

  template<typename Size>
  inline constexpr bool is_data<field::Data<Size>> = true;

  template<typename T>
  inline constexpr bool is_fixed_data = false;

  template<std::size_t N>
  inline constexpr bool is_fixed_data<field::Data<field::Fixed<N>>> = true;

  template<typename T>
  inline constexpr bool is_checksum = false;

  template<std::size_t From, std::size_t To, std::size_t Size, typename Value, typename Codec, auto Calculate>
  inline constexpr bool is_checksum<field::Checksum<From, To, Size, Value, Codec, Calculate>> = true;

  template<typename T>
  inline constexpr bool is_custom = false;

  template<auto ID, std::size_t Size, typename Codec>
  inline constexpr bool is_custom<field::Custom<ID, Size, Codec>> = true;

  template<auto ID, typename T>
  inline constexpr bool is_custom_id = false;

  template<auto ID, std::size_t Size, typename Codec>
  inline constexpr bool is_custom_id<ID, field::Custom<ID, Size, Codec>> = true;
}

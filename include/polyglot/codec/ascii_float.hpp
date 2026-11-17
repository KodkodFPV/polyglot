#pragma once

#include <cstdint>
#include <cstddef>
#include <charconv>
#include <algorithm>
#include <type_traits>
#include <span>
#include <expected>
#include <optional>

#include "error.hpp"


namespace polyglot::codec::ascii
{
  template<typename T, std::chars_format Format = std::chars_format::general, int Precision = 6>
  struct Float
  {
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>, "ascii::Float requires float or double");
    static_assert(Format == std::chars_format::general || Format == std::chars_format::fixed || Format == std::chars_format::scientific, "ascii::Float requires a standard floating-point format");
    static_assert(Precision >= 0, "ascii::Float precision must be nonnegative");

    std::expected<T, Error> decode(std::span<const std::uint8_t> bytes) const noexcept
    {
      const auto zero = std::find(bytes.begin(), bytes.end(), std::uint8_t{0});
      const std::size_t text_size = static_cast<std::size_t>(zero - bytes.begin());
      if (text_size == 0)
        return std::unexpected(Error::InvalidFormat);
      if (std::find_if(zero, bytes.end(), [](std::uint8_t byte) { return byte != 0; }) != bytes.end())
        return std::unexpected(Error::InvalidCharacter);

      const char* first = reinterpret_cast<const char*>(bytes.data());
      const char* last = first + text_size;
      if (text_size == 1 && *first == '-')
        return std::unexpected(Error::InvalidFormat);

      T value{};
      const auto result = std::from_chars(first, last, value, Format);
      if (result.ec == std::errc::result_out_of_range)
        return std::unexpected(Error::ValueOutOfRange);
      if (result.ec != std::errc{} || result.ptr != last)
        return std::unexpected(Error::InvalidCharacter);
      return value;
    }

    std::optional<Error> encode(T value, std::span<std::uint8_t> bytes) const noexcept
    {
      if (bytes.empty())
        return Error::InvalidSize;

      char* first = reinterpret_cast<char*>(bytes.data());
      char* last = reinterpret_cast<char*>(first + bytes.size());
      const auto result = std::to_chars(first, last, value, Format, Precision);
      if (result.ec != std::errc{})
        return Error::ValueOutOfRange;

      std::fill(result.ptr, first + bytes.size(), '\0');

      return std::nullopt;
    }
  };
}

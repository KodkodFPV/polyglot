#pragma once

#include <cstdint>
#include <cstddef>
#include <charconv>
#include <algorithm>
#include <type_traits>
#include <span>
#include <optional>
#include <expected>

#include "error.hpp"


namespace polyglot::codec::ascii
{
  template<typename T>
  struct Decimal
  {
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool> && sizeof(T) <= 8, "ASCIIDecimal requires an integer type of at most 64 bits");

    static constexpr std::optional<Error> is_valid(std::span<const std::uint8_t> bytes)
    {
      if (bytes.empty())
        return Error::InvalidFormat;

      if constexpr (std::is_signed_v<T>)
      {
        if (bytes.front() == '-')
          bytes = bytes.subspan(1);

        if (bytes.empty() || bytes.front() == 0x00)
          return Error::InvalidFormat;
      }

      bool has_digit = false;
      bool padding = false;

      for (std::uint8_t byte : bytes)
      {
        if (byte == 0x00)
          padding = true;
        else if (padding || byte < '0' || byte > '9')
          return Error::InvalidCharacter;
        else
          has_digit = true;
      }

      if (has_digit)
        return std::nullopt;

      return Error::InvalidFormat;
    }

    constexpr std::expected<T, Error> decode(std::span<const std::uint8_t> bytes) const noexcept
    {
      auto invalid = is_valid(bytes);
      if (invalid)
        return std::unexpected(*invalid);

      const auto zero = std::find(bytes.begin(), bytes.end(), std::uint8_t{0});
      const auto digits = bytes.subspan(0, static_cast<std::size_t>(zero - bytes.begin()));
      const char* first = reinterpret_cast<const char*>(digits.data());
      const char* last = first + digits.size();
      T value{};

      const auto result = std::from_chars(first, last, value, 10);
      if (result.ec == std::errc::invalid_argument || result.ptr != last)
        return std::unexpected(Error::InvalidFormat);
      if (result.ec == std::errc::result_out_of_range)
        return std::unexpected(Error::ValueOutOfRange);

      return value;
    }

    constexpr std::optional<Error> encode(T value, std::span<std::uint8_t> bytes) const noexcept
    {
      if (bytes.empty())
        return Error::InvalidFormat;

      char* first = reinterpret_cast<char*>(bytes.data());
      char* last = reinterpret_cast<char*>(first + bytes.size());
      const auto result = std::to_chars(first, last, value, 10);
      if (result.ec == std::errc::value_too_large)
        return Error::ValueOutOfRange;

      const std::size_t written = static_cast<std::size_t>(result.ptr - first);
      for (std::size_t i = written; i < bytes.size(); ++i)
        bytes[i] = 0;
      return std::nullopt;
    }
  };
}

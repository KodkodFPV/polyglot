#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <charconv>
#include <algorithm>
#include <bit>
#include <limits>
#include <array>
#include <span>
#include <expected>
#include <optional>

#include "error.hpp"


namespace polyglot::codec::ascii
{
  namespace hex
  {
    enum class Prefix : std::uint8_t
    {
      None,
      Lower,
      Upper,
    };

    enum class LetterCase : std::uint8_t
    {
      Lower,
      Upper,
      Mixed,
    };
  }

  template<typename T, hex::Prefix Prefix = hex::Prefix::None, hex::LetterCase LetterCase = hex::LetterCase::Upper, bool ZeroPadding = false>
  struct Hexadecimal
  {
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool> && sizeof(T) <= 8, "ascii::Hexadecimal requires an integer type of at most 64 bits");

    static constexpr std::optional<Error> is_valid(std::span<const std::uint8_t> bytes) noexcept
    {
      if (bytes.empty())
        return Error::InvalidFormat;

      if constexpr (std::is_signed_v<T>)
        if (bytes.front() == '-')
          bytes = bytes.subspan(1);

      if (bytes.empty())
        return Error::InvalidFormat;

      if constexpr (Prefix != hex::Prefix::None)
      {
        constexpr auto prefix_letter = Prefix == hex::Prefix::Lower ? 'x' : 'X';
        if (bytes.size() < 3 || bytes[0] != '0' || bytes[1] != prefix_letter)
          return Error::InvalidFormat;
        bytes = bytes.subspan(2);
      }

      for (auto byte : bytes)
      {
        bool valid = byte >= '0' && byte <= '9';
        if constexpr (LetterCase != hex::LetterCase::Upper)
          valid = valid || (byte >= 'a' && byte <= 'f');
        if constexpr (LetterCase != hex::LetterCase::Lower)
          valid = valid || (byte >= 'A' && byte <= 'F');
        if (!valid)
          return Error::InvalidCharacter;
      }

      return std::nullopt;
    }

    constexpr std::expected<T, Error> decode(std::span<const std::uint8_t> bytes) const noexcept
    {
      auto invalid = is_valid(bytes);
      if (invalid)
        return std::unexpected(*invalid);

      auto convert = [](std::span<const std::uint8_t> bytes) -> std::expected<T, Error>
      {
        const char* first = reinterpret_cast<const char*>(bytes.data());
        const char* last = reinterpret_cast<const char*>(bytes.data() + bytes.size());
        T value{};

        const auto result = std::from_chars(first, last, value, 16);
        if (result.ec == std::errc::result_out_of_range)
          return std::unexpected(Error::ValueOutOfRange);
        if (result.ec != std::errc{} || result.ptr != last)
          return std::unexpected(Error::InvalidFormat);

        return value;
      };

      if constexpr (Prefix == hex::Prefix::None)
      {
        return convert(bytes);
      }
      else
      {
        if (bytes.size() > 19)
          return std::unexpected(Error::InvalidFormat);

        const bool negative = std::is_signed_v<T> && bytes.front() == '-';
        bytes = bytes.subspan(negative ? 3 : 2);

        if (!negative)
          return convert(bytes);

        std::array<std::uint8_t, 19> buffer{};
        buffer[0] = '-';
        std::copy(bytes.begin(), bytes.end(), buffer.begin() + 1);

        return convert({buffer.data(), bytes.size() + 1});
      }
    }

    constexpr std::optional<Error> encode(T value, std::span<std::uint8_t> bytes) const noexcept
    {
      constexpr std::size_t prefix_size = Prefix == hex::Prefix::None ? 0 : 2;
      constexpr std::size_t width = 2 * sizeof(T);

      auto hex_digits = [](auto number) -> std::size_t
      {
        return std::max<std::size_t>(1, (std::bit_width(number) + 3) / 4);
      };

      auto convert = [&]<typename V>(V number) -> std::optional<Error>
      {
        char* first = reinterpret_cast<char*>(bytes.data());
        char* last = reinterpret_cast<char*>(first + bytes.size());
        const auto result = std::to_chars(first, last, number, 16);
        if (result.ec != std::errc{})
          return Error::ValueOutOfRange;

        char* end = result.ptr;
        if constexpr (ZeroPadding)
        {
          const auto padding = width - static_cast<std::size_t>(end - first);
          std::copy_backward(first, end, first + width);
          std::fill_n(first, padding, '0');
          end = first + width;
        }

        if constexpr (LetterCase == hex::LetterCase::Upper)
        {
          for (char* p = first; p != end; ++p)
            if (*p >= 'a' && *p <= 'f')
              *p = static_cast<char>(*p - 'a' + 'A');
        }

        return std::nullopt;
      };

      auto add_prefix = [&]()
      {
        if constexpr (Prefix != hex::Prefix::None)
        {
          bytes[0] = '0';
          bytes[1] = Prefix == hex::Prefix::Lower ? 'x' : 'X';
          bytes = bytes.subspan(2);
        }
      };

      if constexpr (std::is_signed_v<T>)
      {
        using U = std::make_unsigned_t<T>;
        U absolute = static_cast<U>(value);
        const bool negative = value < 0;
        if (negative)
          absolute = static_cast<U>(U{} - absolute);

        std::size_t required = prefix_size + (ZeroPadding ? width : hex_digits(absolute));
        if (negative)
          ++required;
        if (bytes.size() < required)
          return Error::ValueOutOfRange;

        if (negative)
        {
          bytes.front() = '-';
          bytes = bytes.subspan(1);
        }
        add_prefix();
        return convert(absolute);
      }
      else
      {
        const std::size_t required = prefix_size + (ZeroPadding ? width : hex_digits(value));
        if (bytes.size() < required)
          return Error::ValueOutOfRange;

        add_prefix();
        return convert(value);
      }
    }
  };
}

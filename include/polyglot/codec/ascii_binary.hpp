#pragma once

#include <cstdint>
#include <cstddef>
#include <bit>
#include <type_traits>
#include <ranges>
#include <charconv>
#include <algorithm>
#include <span>
#include <optional>
#include <expected>

#include "error.hpp"


namespace polyglot::codec::ascii
{
  namespace bin
  {
    enum class Prefix : std::uint8_t
    {
      None,
      Lower,
      Upper,
    };
  }

  template<typename T, bin::Prefix Prefix = bin::Prefix::None>
  struct Binary
  {
    static_assert(std::is_same_v<T, std::uint8_t>  || std::is_same_v<T, std::int8_t>  ||
                  std::is_same_v<T, std::uint16_t> || std::is_same_v<T, std::int16_t> ||
                  std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::int32_t> ||
                  std::is_same_v<T, std::uint64_t> || std::is_same_v<T, std::int64_t>,
                  "ascii::Binary requires an 8/16/32/64-bit integer");

    using U = std::make_unsigned_t<T>;
    static constexpr std::size_t bit_count = sizeof(T) * 8;

    static constexpr bool accepts_size(std::size_t size) noexcept
    {
      if constexpr (Prefix == bin::Prefix::None)
        return size == bit_count;
      else if constexpr ((Prefix == bin::Prefix::Lower) || (Prefix == bin::Prefix::Upper))
        return size == (bit_count + 2);

      return false;
    }

    static constexpr std::expected<std::uint8_t, Error> is_valid(std::span<const std::uint8_t> bytes)
    {
      if (!accepts_size(bytes.size()))
        return std::unexpected(Error::InvalidSize);

      std::uint8_t offset{0};

      if constexpr (Prefix == bin::Prefix::Lower)
      {
        if ((bytes[0] == '0') && (bytes[1] == 'b'))
        {
          bytes = bytes.subspan(2);
          offset = 2;
        }
        else
        {
          return std::unexpected(Error::InvalidFormat);
        }
      }
      else if constexpr (Prefix == bin::Prefix::Upper)
      {
        if ((bytes[0] == '0') && (bytes[1] == 'B'))
        {
          bytes = bytes.subspan(2);
          offset = 2;
        }
        else
        {
          return std::unexpected(Error::InvalidFormat);
        }
      }

      for (auto byte : bytes)
        if ((byte != '0') && (byte != '1'))
          return std::unexpected(Error::InvalidCharacter);

      return offset;
    }

    constexpr std::expected<T, Error> decode(std::span<const std::uint8_t> bytes) const noexcept
    {
      if (!accepts_size(bytes.size()))
        return std::unexpected(Error::InvalidSize);

      auto offset = is_valid(bytes);
      if (!offset)
        return std::unexpected(offset.error());
      bytes = bytes.subspan(*offset);

      const char* first = reinterpret_cast<const char*>(bytes.data());
      const char* last = first + bit_count;
      U bits{};

      const auto result = std::from_chars(first, last, bits, 2);
      if (result.ec == std::errc::result_out_of_range)
        return std::unexpected(Error::ValueOutOfRange);

      return std::bit_cast<T>(bits);
    }

    constexpr std::optional<Error> encode(T value, std::span<std::uint8_t> bytes) const noexcept
    {
      if (!accepts_size(bytes.size()))
        return Error::InvalidSize;

      if constexpr (Prefix == bin::Prefix::Lower)
      {
        bytes[0] = '0';
        bytes[1] = 'b';
        bytes = bytes.subspan(2);
      }
      else if constexpr (Prefix == bin::Prefix::Upper)
      {
        bytes[0] = '0';
        bytes[1] = 'B';
        bytes = bytes.subspan(2);
      }

      char* first = reinterpret_cast<char*>(bytes.data());
      char* last = reinterpret_cast<char*>(first + bit_count);
      const auto result = std::to_chars(first, last, std::bit_cast<U>(value), 2);
      if (result.ec != std::errc{})
        return Error::ValueOutOfRange;

      const std::size_t written = static_cast<std::size_t>(result.ptr - first);
      const std::size_t padding = bytes.size() - written;
      std::copy_backward(first, result.ptr, first + bytes.size());
      std::fill_n(first, padding, '0');

      return std::nullopt;
    }
  };
}

#pragma once

#include <cstdint>
#include <cstddef>
#include <bit>
#include <type_traits>
#include <span>
#include <expected>
#include <optional>

#include "error.hpp"


namespace polyglot::codec::bin
{
  template<typename T>
  struct IntegerLE
  {
    static_assert(std::is_same_v<T, std::uint8_t>  || std::is_same_v<T, std::int8_t>  ||
                  std::is_same_v<T, std::uint16_t> || std::is_same_v<T, std::int16_t> ||
                  std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::int32_t> ||
                  std::is_same_v<T, std::uint64_t> || std::is_same_v<T, std::int64_t>,
                  "bin::IntegerLE requires an 8, 16, 32, or 64-bit integer type");

    static constexpr bool accepts_size(std::size_t size) noexcept
    {
      return size >= 1 && size <= sizeof(T);
    }

    constexpr std::expected<T, Error> decode(std::span<const std::uint8_t> bytes) const noexcept
    {
      if (!accepts_size(bytes.size()))
        return std::unexpected(Error::InvalidSize);

      using U = std::make_unsigned_t<T>;
      U bits{};

      for (std::size_t i = 0; i < bytes.size(); ++i)
        bits |= static_cast<U>(bytes[i]) << (i * 8);

      if constexpr (std::is_signed_v<T>)
      {
        const auto high_byte = bytes.back();

        if (high_byte & 0x80)
        {
          for (std::size_t i = bytes.size(); i < sizeof(T); ++i)
            bits |= static_cast<U>(0xFF) << (i * 8);
        }

        return std::bit_cast<T>(bits);
      }
      else
        return static_cast<T>(bits);
    }

    constexpr std::optional<Error> encode(T value, std::span<std::uint8_t> bytes) const noexcept
    {
      if (!accepts_size(bytes.size()))
        return Error::InvalidSize;

      if (bytes.size() < sizeof(T))
      {
        const auto bit_count = bytes.size() * 8;
        if constexpr (std::is_signed_v<T>)
        {
          const auto limit = std::int64_t{1} << (bit_count - 1);
          if (static_cast<std::int64_t>(value) < -limit || static_cast<std::int64_t>(value) >= limit)
            return Error::ValueOutOfRange;
        }
        else if (static_cast<std::uint64_t>(value) >= (std::uint64_t{1} << bit_count))
          return Error::ValueOutOfRange;
      }

      using U = std::make_unsigned_t<T>;
      const auto bits = std::bit_cast<U>(value);

      for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<std::uint8_t>(bits >> (i * 8));

      return std::nullopt;
    }
  };
}

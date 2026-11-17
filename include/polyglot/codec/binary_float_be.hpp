#pragma once

#include <cstdint>
#include <cstddef>
#include <bit>
#include <limits>
#include <type_traits>
#include <span>
#include <expected>
#include <optional>

#include "error.hpp"


namespace polyglot::codec::bin
{
  template<typename T>
  struct FloatBE
  {
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>, "BinaryBEFloat requires float or double");
    static_assert(std::numeric_limits<T>::is_iec559 && ((std::is_same_v<T, float> && sizeof(T) == 4) || (std::is_same_v<T, double> && sizeof(T) == 8)),
                  "bin::FloatBE requires IEEE 754 binary32 or binary64 representation");

    using Bits = std::conditional_t<std::is_same_v<T, float>, std::uint32_t, std::uint64_t>;

    static constexpr bool accepts_size(std::size_t size) noexcept
    {
      return size == sizeof(T);
    }

    constexpr std::expected<T, Error> decode(std::span<const std::uint8_t> bytes) const noexcept
    {
      if (!accepts_size(bytes.size()))
        return std::unexpected(Error::InvalidSize);

      Bits bits{};
      for (auto byte : bytes)
        bits = static_cast<Bits>((bits << 8) | byte);

      return std::bit_cast<T>(bits);
    }

    constexpr std::optional<Error> encode(T value, std::span<std::uint8_t> bytes) const noexcept
    {
      if (!accepts_size(bytes.size()))
        return Error::InvalidSize;

      const auto bits = std::bit_cast<Bits>(value);
      for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<std::uint8_t>(bits >> ((bytes.size() - i - 1) * 8));

      return std::nullopt;
    }
  };
}

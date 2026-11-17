#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <string_view>
#include <expected>
#include <optional>

#include "error.hpp"


namespace polyglot::codec::ascii
{
  struct Text
  {
    static constexpr bool accepts_size(std::size_t size) noexcept { return size > 0; }

    constexpr std::expected<std::string_view, Error> decode(std::span<const std::uint8_t> bytes) const noexcept
    {
      if (bytes.empty())
        return std::unexpected(Error::InvalidSize);

      std::size_t text_size = bytes.size();
      bool padding = false;

      for (std::size_t i = 0; i < bytes.size(); ++i)
      {
        if (bytes[i] > 0x7F)
          return std::unexpected(Error::InvalidCharacter);
        if (bytes[i] == 0)
        {
          if (!padding)
            text_size = i;
          padding = true;
        }
        else if (padding)
          return std::unexpected(Error::InvalidFormat);
      }

      return std::string_view{reinterpret_cast<const char*>(bytes.data()), text_size};
    }

    constexpr std::optional<Error> encode(std::string_view text, std::span<std::uint8_t> bytes) const noexcept
    {
      if (bytes.empty())
        return Error::InvalidSize;
      if (text.size() > bytes.size())
        return Error::ValueOutOfRange;

      for (char character : text)
        if (character == 0 || static_cast<unsigned char>(character) > 0x7F)
          return Error::InvalidCharacter;

      for (std::size_t i = 0; i < text.size(); ++i)
        bytes[i] = static_cast<std::uint8_t>(text[i]);
      for (std::size_t i = text.size(); i < bytes.size(); ++i)
        bytes[i] = 0;

      return std::nullopt;
    }
  };
}

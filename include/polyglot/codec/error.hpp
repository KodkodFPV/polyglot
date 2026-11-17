#pragma once

#include <cstdint>


namespace polyglot::codec
{
  enum class Error : std::uint8_t
  {
    InvalidSize,
    InvalidCharacter,
    InvalidFormat,
    ValueOutOfRange,
  };
}

#pragma once

#include <cstdint>


namespace polyglot::err
{
  enum class ID : std::uint8_t
  {
    InvalidConfiguration,
    UnsupportedCodecWidth,
    NotImplemented,
    FrameNotReady,
    InvalidStart,
    InvalidStop,
    InvalidDelimetr,
    InvalidFrameLength,
    InvalidCustomField,
    InvalidChecksum,
    FrameLengthNotRead,
  };
};

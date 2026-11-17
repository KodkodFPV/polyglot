#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <tuple>

#include <polyglot/error.hpp>

#include <polyglot/schema/fields.hpp>
#include <polyglot/schema/frame.hpp>


namespace polyglot::schema::check
{
  namespace config_detail
  {
    template<typename Codec>
    constexpr bool codec_accepts_size(std::size_t size)
    {
      if constexpr (requires { Codec::accepts_size(size); })
        return Codec::accepts_size(size);
      else
        return true;
    }

    template<typename Field>
    constexpr std::optional<err::ID> check_field(const Field&)
    {
      return std::nullopt;
    }

    template<std::size_t From, std::size_t To, std::size_t Size, typename Codec>
    constexpr std::optional<err::ID> check_field(const field::Length<From, To, Size, Codec>&)
    {
      if (!codec_accepts_size<Codec>(Size)) return err::ID::UnsupportedCodecWidth;
      return std::nullopt;
    }

    template<std::size_t From, std::size_t To, std::size_t Size, typename Value, typename Codec, auto Calculate>
    constexpr std::optional<err::ID> check_field(const field::Checksum<From, To, Size, Value, Codec, Calculate>&)
    {
      if (!codec_accepts_size<Codec>(Size)) return err::ID::UnsupportedCodecWidth;
      return std::nullopt;
    }

    template<auto Id, std::size_t Size, typename Codec>
    constexpr std::optional<err::ID> check_field(const field::Custom<Id, Size, Codec>&)
    {
      if (!codec_accepts_size<Codec>(Size)) return err::ID::UnsupportedCodecWidth;
      return std::nullopt;
    }
  }

  template<typename... Fields>
  constexpr std::optional<err::ID> validate_config(const Frame<Fields...>& frame)
  {
    std::optional<err::ID> error;
    std::apply([&error](const auto&... fields)
    {
      ((error = error ? error : config_detail::check_field(fields)), ...);
    }, frame.fields);
    return error;
  }
}

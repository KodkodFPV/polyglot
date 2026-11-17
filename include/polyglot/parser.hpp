#pragma once

#include <cstdint>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <array>
#include <span>
#include <optional>
#include <expected>
#include <tuple>

#include "error.hpp"

#include "schema/check/structure.hpp"
#include "schema/check/config.hpp"
#include "schema/traits.hpp"


namespace polyglot
{
  template<typename Frame>
  class Parser
  {
  public:
    explicit Parser(Frame frame)
      : m_frame(std::move(frame))
    {
      schema::check::validate_structure(std::type_identity<Frame>{});
      m_error = schema::check::validate_config(m_frame);
    }

    bool ok() const noexcept { return !m_error.has_value(); }
    std::optional<err::ID> error() const noexcept { return m_error; }

    std::expected<std::size_t, err::ID> process(std::span<const std::uint8_t> data)
    {
      if (!ok())
        return std::unexpected(err::ID::InvalidConfiguration);

      auto remaining = data;
      std::optional<err::ID> error_process;

      auto process_next = [&](auto& field)
      {
        error_process = process_field(field, remaining);

        return !error_process;
      };

      std::apply([&](auto&... fields)
      {
        (process_next(fields) && ...);
      }, m_frame.fields);

      if (error_process)
        return std::unexpected(*error_process);

      if constexpr (schema::traits::position_checksum<Frame> != schema::traits::npos)
      {
        if (auto error_checksum = validate_checksum(data))
          return std::unexpected(*error_checksum);
      }

      return data.size() - remaining.size();
    }

  private:
    static constexpr std::size_t FrameElementsNum = std::tuple_size_v<decltype(std::declval<Frame>().fields)>;

    template<typename T>
    static consteval auto checksum_value_type()
    {
      if constexpr (schema::traits::position_checksum<T> == schema::traits::npos)
        return std::type_identity<std::monostate>{};
      else
      {
        using Checksum = std::remove_cvref_t<decltype(schema::traits::field_checksum(std::declval<const T&>()))>;

        return std::type_identity<typename Checksum::ValueT>{};
      }
    }

    using ChecksumValue = typename decltype(checksum_value_type<Frame>())::type;

    template<typename Field>
    static constexpr auto custom_fields_tuple()
    {
      if constexpr (schema::traits::is_custom<Field>)
      {
        using Result = decltype(std::declval<const Field&>().codec.decode(schema::field::ByteView{}));

        return std::tuple<typename Result::value_type>{};
      }
      else
        return std::tuple<>{};
    }

    using CustomFields = decltype([]<typename... Fields>(std::type_identity<schema::Frame<Fields...>>)
    {
      return std::tuple_cat(custom_fields_tuple<Fields>()...);
    }(std::type_identity<Frame>{}));

    Frame                  m_frame;
    CustomFields           m_custom_fields;
    ChecksumValue          m_checksum_value;
    std::optional<err::ID> m_error;

    std::optional<std::size_t>                   m_length;
    std::optional<std::span<const std::uint8_t>> m_data;

  private:
    std::expected<std::span<const std::uint8_t>, err::ID> read_bytes(std::span<const std::uint8_t>& remaining, std::size_t count)
    {
      if (remaining.size() < count)
        return std::unexpected(err::ID::FrameNotReady);

      auto bytes = remaining.first(count);
      remaining = remaining.subspan(count);

      return bytes;
    }

    template<std::size_t From, std::size_t To, std::size_t Excluded = schema::traits::npos>
    constexpr std::expected<std::size_t, err::ID> sum_field_sizes() const
    {
      static_assert(From <= To && To < FrameElementsNum);
      static_assert(Excluded == schema::traits::npos || (From <= Excluded && Excluded <= To));

      constexpr auto fixed_bytes = []<std::size_t... I>(std::index_sequence<I...>)
      {
        return (std::size_t{0} + ... +
        ([]{
          using Field = std::tuple_element_t<From + I, decltype(std::declval<Frame>().fields)>;

          if constexpr (Field::dynamic_size)
            return std::size_t{0};
          else if constexpr ((From + I) == Excluded)
            return std::size_t{0};
          else
            return Field::size;
        }()));
      }(std::make_index_sequence<To - From + 1>{});

      using FieldsTuple = decltype(std::declval<Frame>().fields);

      constexpr auto dynamic_indexes = []<std::size_t... I>(std::index_sequence<I...>)
      {
        constexpr std::size_t count = (std::size_t{0} + ... + static_cast<std::size_t>(From + I != Excluded && std::tuple_element_t<From + I, FieldsTuple>::dynamic_size));

        std::array<std::size_t, count> indexes{};
        std::size_t next = 0;

        (([&] {
          if constexpr (From + I != Excluded && std::tuple_element_t<From + I, FieldsTuple>::dynamic_size)
            indexes[next++] = From + I;
        }()), ...);

        return indexes;
      }(std::make_index_sequence<To - From + 1>{});

      std::expected<std::size_t, err::ID> size = fixed_bytes;

      [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        ((size = size.and_then([&](std::size_t total) -> std::expected<std::size_t, err::ID>
        {
          const auto& resolved = std::get<dynamic_indexes[I]>(m_frame.fields).resolved_size;

          if (!resolved)
            return std::unexpected(err::ID::FrameNotReady);

          if (*resolved > std::numeric_limits<std::size_t>::max() - total)
            return std::unexpected(err::ID::InvalidFrameLength);

          return total + *resolved;
        })), ...);
      }(std::make_index_sequence<dynamic_indexes.size()>{});

      return size;
    }

    template<typename Length>
    std::expected<std::size_t, err::ID> data_size_from_length(std::size_t frame_length) const
    {
      static_assert(schema::traits::is_length_field<Length>);

      constexpr auto data_pos = schema::traits::position_data<Frame>;

      static_assert(Length::from <= data_pos && data_pos <= Length::to);

      auto other_bytes = sum_field_sizes<Length::from, Length::to, data_pos>();
      if (!other_bytes)
        return std::unexpected(other_bytes.error());

      if (frame_length < *other_bytes)
        return std::unexpected(err::ID::InvalidFrameLength);

      return frame_length - *other_bytes;
    }

    std::expected<std::size_t, err::ID> data_size_from_marker(std::span<const std::uint8_t> remaining) const
    {
      constexpr auto data_pos = schema::traits::position_data<Frame>;
      constexpr auto delimiter_pos = schema::traits::position_delimiter<Frame>;
      constexpr auto stop_pos = schema::traits::position_stop<Frame>;
      constexpr auto boundary_pos = delimiter_pos != schema::traits::npos ? delimiter_pos : stop_pos;

      static_assert(boundary_pos != schema::traits::npos);

      const auto marker = std::get<boundary_pos>(m_frame.fields).bytes;
      const auto found = std::ranges::search(remaining, marker);

      if (found.empty())
        return std::unexpected(err::ID::FrameNotReady);

      const auto bytes_before_marker = static_cast<std::size_t>(found.begin() - remaining.begin());

      if constexpr (data_pos + 1 == boundary_pos)
        return bytes_before_marker;
      else
      {
        auto trailing_size = sum_field_sizes<data_pos + 1, boundary_pos - 1>();
        if (!trailing_size)
          return std::unexpected(trailing_size.error());

        if (bytes_before_marker < *trailing_size)
          return std::unexpected(err::ID::InvalidFrameLength);

        return bytes_before_marker - *trailing_size;
      }
    }

    std::optional<err::ID> validate_checksum(std::span<const std::uint8_t> data)
    {
      using Checksum = std::remove_cvref_t<decltype(schema::traits::field_checksum(m_frame))>;

      std::expected<std::size_t, err::ID> from = std::size_t{0};
      if constexpr (Checksum::from > 0)
        from = sum_field_sizes<0, Checksum::from - 1>();

      auto to = sum_field_sizes<0, Checksum::to>();

      if (!from)
        return from.error();

      if (!to)
        return to.error();

      auto bytes = data.subspan(*from, *to - *from);
      if (Checksum::calculate(bytes) != m_checksum_value)
        return err::ID::InvalidChecksum;

      return std::nullopt;
    }


    std::optional<err::ID> process_marker(std::span<const std::uint8_t> marker, std::span<const std::uint8_t>& remaining, err::ID mismatch)
    {
      auto bytes = read_bytes(remaining, marker.size());
      if (!bytes)
        return bytes.error();

      if (!std::ranges::equal(*bytes, marker))
        return mismatch;

      return std::nullopt;
    }

    template<std::size_t N>
    std::optional<err::ID> process_field(schema::field::Start<N>& field, std::span<const std::uint8_t>& remaining)
    {
      return process_marker(field.bytes, remaining, err::ID::InvalidStart);
    }

    template<std::size_t N>
    std::optional<err::ID> process_field(schema::field::Stop<N>& field, std::span<const std::uint8_t>& remaining)
    {
      return process_marker(field.bytes, remaining, err::ID::InvalidStop);
    }

    template<std::size_t N>
    std::optional<err::ID> process_field(schema::field::Delimiter<N>& field, std::span<const std::uint8_t>& remaining)
    {
      return process_marker(field.bytes, remaining, err::ID::InvalidDelimetr);
    }

    template<std::size_t From, std::size_t To, std::size_t Size, typename Codec>
    std::optional<err::ID> process_field(schema::field::Length<From, To, Size, Codec>& field, std::span<const std::uint8_t>& remaining)
    {
      auto bytes = read_bytes(remaining, field.size);
      if (!bytes)
        return bytes.error();

      auto value = field.codec.decode(*bytes);
      if (!value || !std::in_range<std::size_t>(*value))
        return err::ID::InvalidFrameLength;

      using Data = std::remove_cvref_t<decltype(schema::traits::field_data(m_frame))>;
      if constexpr (schema::traits::is_fixed_data<Data>)
      {
        if (static_cast<std::size_t>(*value) != Data::size)
          return err::ID::InvalidFrameLength;
      }

      m_length = static_cast<std::size_t>(*value);

      return std::nullopt;
    }

    template<typename Size>
    std::optional<err::ID> process_field(schema::field::Data<Size>& field, std::span<const std::uint8_t>& data)
    {
      auto size = [&]() -> std::expected<std::size_t, err::ID>
      {
        if constexpr (schema::traits::is_fixed_data<schema::field::Data<Size>>)
        {
          return Size::size;
        }
        else if constexpr (schema::traits::length_covers_data<Frame>)
        {
          if (!m_length)
            return std::unexpected(err::ID::FrameLengthNotRead);

          using Length = std::tuple_element_t<schema::traits::position_length<Frame>, decltype(m_frame.fields)>;

          return data_size_from_length<Length>(*m_length);
        }
        else
        {
          return data_size_from_marker(data);
        }
      }();

      if (!size)
        return size.error();

      auto bytes = read_bytes(data, *size);
      if (!bytes)
        return bytes.error();

      m_data = *bytes;

      if constexpr (std::is_same_v<Size, schema::field::Auto>)
        field.resolved_size = *size;

      return std::nullopt;
    }

    template<std::size_t From, std::size_t To, std::size_t Size, typename Value, typename Codec, auto Calculate>
    std::optional<err::ID> process_field(schema::field::Checksum<From, To, Size, Value, Codec, Calculate>& field, std::span<const std::uint8_t>& remaining)
    {
      auto bytes = read_bytes(remaining, Size);
      if (!bytes)
        return bytes.error();

      auto decoded = field.codec.decode(*bytes);
      if (!decoded)
        return err::ID::InvalidChecksum;

      m_checksum_value = std::move(*decoded);

      return std::nullopt;
    }

    template<auto ID, std::size_t Size, typename Codec>
    std::optional<err::ID> process_field(schema::field::Custom<ID, Size, Codec>& field, std::span<const std::uint8_t>& remaining)
    {
      constexpr auto index = []<typename... Fields>(std::type_identity<schema::Frame<Fields...>>)
      {
        constexpr std::array custom{schema::traits::is_custom<Fields>...};
        constexpr auto position = schema::traits::position_custom<ID, Frame>;

        return std::count(custom.begin(), custom.begin() + position, true);
      }(std::type_identity<Frame>{});

      auto bytes = read_bytes(remaining, Size);
      if (!bytes)
        return bytes.error();

      auto decoded = field.codec.decode(*bytes);
      if (!decoded)
        return err::ID::InvalidCustomField;

      std::get<index>(m_custom_fields) = std::move(*decoded);

      return std::nullopt;
    }
  };
}

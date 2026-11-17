#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <span>
#include <optional>


namespace polyglot::schema::field
{
  using ByteView = std::span<const std::uint8_t>;

  // Start field
  template<std::size_t N>
  struct Start
  {
    static_assert(N > 0, "Start width must be positive");

    static constexpr bool        dynamic_size = false;
    static constexpr std::size_t size = N;

    ByteView bytes{};
  };
  // ===========

  // Stop field
  template<std::size_t N>
  struct Stop
  {
    static_assert(N > 0, "Stop width must be positive");

    static constexpr bool        dynamic_size = false;
    static constexpr std::size_t size = N;

    ByteView bytes{};
  };
  // ===========

  // Delimetr field
  template<std::size_t N>
  struct Delimiter
  {
    static_assert(N > 0, "Delimiter width must be positive");

    static constexpr bool        dynamic_size = false;
    static constexpr std::size_t size = N;

    ByteView bytes{};
  };
  // ==============

  // Length field
  template<std::size_t From, std::size_t To, std::size_t Size, typename Codec>
  struct Length
  {
    static_assert(Size > 0, "Length size must be positive");

    static constexpr bool        dynamic_size = false;
    static constexpr std::size_t size = Size;
    static constexpr std::size_t from = From;
    static constexpr std::size_t to = To;

    Codec codec{};
  };
  // ============

  // Data field
  struct Auto {};

  template<std::size_t N>
  struct Fixed
  {
    static constexpr std::size_t size = N;
  };

  template<typename Size = Auto>
  struct Data
  {
    static_assert(std::is_same_v<Size, Auto>, "Data size must be Auto or Fixed<N>");
  };

  template<std::size_t N>
  struct Data<Fixed<N>>
  {
    static constexpr bool        dynamic_size = false;
    static constexpr std::size_t size = N;

    ByteView bytes{};
  };

  template<>
  struct Data<Auto>
  {
    using Size = std::optional<std::size_t>;

    static constexpr bool dynamic_size = true;

    Size     resolved_size;
    ByteView bytes{};
  };
  // ==========

  // Checksum field
  template<std::size_t From, std::size_t To, std::size_t Size, typename Value, typename Codec, auto Calculate>
  struct Checksum
  {
    static_assert(Size > 0, "Checksum size must be positive");

    using ValueT = Value;
    using DecodeResult = decltype(std::declval<const Codec&>().decode(ByteView{}));

    static_assert(std::is_same_v<typename DecodeResult::value_type, Value>, "Checksum codec must decode to Value");
    static_assert(std::is_same_v<std::invoke_result_t<decltype(Calculate), ByteView>, Value>, "Checksum calculation must return Value");

    static constexpr bool        dynamic_size = false;
    static constexpr std::size_t from = From;
    static constexpr std::size_t to = To;
    static constexpr std::size_t size = Size;
    static constexpr auto        calculate = Calculate;

    Codec codec{};
  };
  // ==============

  // Application field
  template<auto ID, std::size_t Size, typename Codec>
  struct Custom
  {
    static_assert(Size > 0, "Custom field size must be positive");

    static constexpr bool        dynamic_size = false;
    static constexpr std::size_t size = Size;
    static constexpr auto        id = ID;

    Codec codec{};
  };
  // =================
}

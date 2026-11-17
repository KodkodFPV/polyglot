#pragma once

#include <type_traits>
#include <tuple>


namespace polyglot::schema
{
  template<typename... Fields>
  struct Frame
  {
    std::tuple<Fields...> fields;

    constexpr Frame(Fields... values)
      : fields(std::move(values)...)
    {

    }
  };
}

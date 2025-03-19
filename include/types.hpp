#pragma once

#include <string>
#include <type_traits>

namespace detail {

/// SFINAE helper struct to check if type T is a string with any allocator.
template <typename T>
struct is_string : std::false_type {};

/// std::basic_string with any allocator are a string.
template <typename Alloc>
struct is_string<std::basic_string<char, std::char_traits<char>, Alloc>>
    : std::true_type {};

/// Helper template to simplify usage C++17 style.
template <typename T>
inline constexpr bool is_string_v = is_string<T>::value;

}  // namespace detail

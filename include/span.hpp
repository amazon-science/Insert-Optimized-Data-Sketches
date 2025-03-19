#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace std {

namespace detail {

template <typename T, typename C>
using has_data =
    std::is_convertible<std::decay_t<decltype(std::declval<C&>().data())>*,
                        T* const*>;

template <typename T, typename C>
using has_size = std::is_integral<decltype(std::declval<C&>().size())>;

}  // namespace detail

/// Default value for the Extent of span. No semantics for now.
inline constexpr size_t dynamic_extent = -1;

/// Implementation that is a subset of std::span that we will get with C++20.
/// This implementations's API is supposed to be compatible with std::span.
/// NOTE:
///   - This implementation does not have support for Extent. All span objects
///   are dynamic_extent.
///   - There are a few extra constructors because we don't have ranges yet.
template <typename T, size_t Extent = dynamic_extent>
class span {
 public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = element_type&;
  using const_reference = const element_type&;
  using iterator = pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;

  constexpr span() noexcept = default;

  constexpr span(pointer array, size_type length) noexcept
      : ptr_(array), len_(length) {}

  /// Implicit conversion constructor for arrays.
  template <size_t N>
  constexpr span(T (&a)[N]) noexcept  // NOLINT(runtime/explicit)
      : span(a, N) {}

  /// Implicit conversion constructor for mutable containers.
  template <typename C,
            typename = std::enable_if_t<detail::has_data<T, C>::value &&
                                        detail::has_size<T, C>::value &&
                                        !std::is_const<T>::value>>
  constexpr span(C& container) noexcept  // NOLINT(runtime/explicit)
      : span(container.data(), container.size()) {}

  /// Implicit conversion constructor for mutable containers.
  template <typename C,
            typename = std::enable_if_t<detail::has_data<T, C>::value &&
                                        detail::has_size<T, C>::value &&
                                        std::is_const<T>::value>>
  constexpr span(const C& container) noexcept  // NOLINT(runtime/explicit)
      : span(container.data(), container.size()) {}

  /// Constructs a span that is a view over the range [first, first + count).
  template <typename It>
  span(It first, size_type count) noexcept
      : ptr_(std::addressof(*first)), len_(count) {
    static_assert(
        std::is_convertible_v<typename std::iterator_traits<It>::reference,
                              reference>,
        "Iterator reference type must be compatible with span's reference "
        "type");
  }

  [[nodiscard]] constexpr size_type size() const noexcept { return len_; }

  [[nodiscard]] constexpr size_type size_bytes() const noexcept {
    return len_ * sizeof(element_type);
  }

  [[nodiscard]] constexpr bool empty() const noexcept { return len_ == 0; }

  [[nodiscard]] constexpr reference front() const noexcept { return *ptr_; }

  [[nodiscard]] constexpr reference back() const noexcept {
    return ptr_[len_ - 1];
  }

  [[nodiscard]] constexpr reference operator[](size_type i) const noexcept {
    return ptr_[i];
  }

  [[nodiscard]] constexpr pointer data() const noexcept { return ptr_; }

  [[nodiscard]] constexpr iterator begin() const noexcept {
    return iterator(ptr_);
  }

  [[nodiscard]] constexpr iterator end() const noexcept {
    return iterator(ptr_ + len_);
  }

  [[nodiscard]] constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator(end());
  }

  [[nodiscard]] constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator(begin());
  }

  [[nodiscard]] constexpr span first(size_type count) const noexcept {
    return {ptr_, count};
  }

  [[nodiscard]] constexpr span last(size_type count) const noexcept {
    return {ptr_ + (len_ - count), count};
  }

  [[nodiscard]] constexpr span subspan(
      size_type offset = 0, size_type count = dynamic_extent) const noexcept {
    if (count == dynamic_extent) count = this->size() - offset;
    return {ptr_ + offset, count};
  }

 private:
  pointer ptr_ = nullptr;
  size_type len_ = 0;
  /// The implementation does not support Extent, make sure it's the default.
  static_assert(Extent == dynamic_extent);
};

}  // namespace std

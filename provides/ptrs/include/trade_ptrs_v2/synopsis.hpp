#pragma once

#include "trade_ptrs_v2/private.hpp"

#include <optional>

// =============================================================================

namespace trade_ptrs_v2 {

template <class Type> struct sole_ptr : Private::sole_ptr<Type> {
  using pointer = typename ptr_traits<Type>::pointer;
  using element_type = typename ptr_traits<Type>::element_type;

  pointer operator->() const noexcept;

  std::add_lvalue_reference_t<element_type> operator*() const;

  element_type &operator[](std::size_t i) const;

  void swap(sole_ptr &that) noexcept;

  sole_ptr &operator=(sole_ptr &&that);
  sole_ptr &operator=(std::nullptr_t);

  bool operator!() const noexcept;
  operator bool() const noexcept;

  pointer get() const noexcept;

  ~sole_ptr() = delete;

  sole_ptr() = delete;
  sole_ptr(const sole_ptr &) = delete;

  sole_ptr &operator=(const sole_ptr &) = delete;
};

template <class Type>
void swap(sole_ptr<Type> &lhs, sole_ptr<Type> &rhs) noexcept;

} // namespace trade_ptrs_v2

// =============================================================================

namespace trade_ptrs_v2 {

template <class Type> struct ptr_traits {
  using pointer = typename Private::ptr_traits<Type>::pointer;
  using element_type = typename Private::ptr_traits<Type>::element_type;

  static void destroy(pointer ptr) noexcept;

  static pointer arrow(pointer ptr) noexcept;

  static std::add_lvalue_reference_t<element_type> dereference(pointer ptr);

  static element_type &index(pointer ptr, std::size_t nth);
};

} // namespace trade_ptrs_v2

// =============================================================================

namespace trade_core_v2 {

template <class Type> struct atom_traits<trade_ptrs_v2::sole_ptr<Type>> {
  using value_type = trade_ptrs_v2::sole_ptr<Type>;
  using master_type = trade_ptrs_v2::Private::master_ptr<Type>;
  using readonly_type = typename trade_ptrs_v2::ptr_traits<Type>::pointer;

  static constexpr bool is_atomic = true;
  static constexpr bool is_trivial = false;

  static readonly_type load(const master_type &from) noexcept;

  static void snap(const master_type &from,
                   std::aligned_union_t<1, value_type> &to);

  static std::optional<master_type> commit(value_type &from,
                                           master_type &to) noexcept;
};

} // namespace trade_core_v2

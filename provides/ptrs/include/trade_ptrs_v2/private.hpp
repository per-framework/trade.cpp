#pragma once

#include "trade_core_v2/synopsis.hpp"

namespace trade_ptrs_v2 {

template <class Type> struct sole_ptr;

template <class Type> struct ptr_traits;

class Private {
  template <class> friend struct trade_ptrs_v2::sole_ptr;

  template <class> friend struct trade_core_v2::atom_traits;

  template <class Type> class sole_ptr;

  template <class Type> struct master_ptr;

public:
  template <class Type> struct ptr_traits;
};

} // namespace trade_ptrs_v2

template <class Type> class trade_ptrs_v2::Private::sole_ptr {
  friend struct trade_ptrs_v2::sole_ptr<Type>;
  friend struct trade_core_v2::atom_traits<trade_ptrs_v2::sole_ptr<Type>>;

  typename trade_ptrs_v2::ptr_traits<Type>::pointer m_pointer;
};

template <class Type> struct trade_ptrs_v2::Private::master_ptr {
  friend struct trade_core_v2::atom_traits<trade_ptrs_v2::sole_ptr<Type>>;

  using pointer = typename trade_ptrs_v2::ptr_traits<Type>::pointer;

  std::atomic<pointer> m_pointer;

  ~master_ptr() noexcept;
  master_ptr(pointer pointer = nullptr) noexcept;

  pointer load() const noexcept;
  void store(pointer pointer) noexcept;

  master_ptr(const master_ptr &) = delete;
  master_ptr &operator=(const master_ptr &) = delete;
};

//

template <class Object> struct trade_ptrs_v2::Private::ptr_traits {
  using pointer = Object *;
  using element_type = Object;

  static void destroy(pointer ptr) noexcept;

  static Object *arrow(pointer ptr) noexcept;

  static std::add_lvalue_reference_t<element_type> dereference(pointer ptr);
};

template <class Element> struct trade_ptrs_v2::Private::ptr_traits<Element[]> {
  using pointer = Element *;
  using element_type = Element;

  static void destroy(pointer ptr) noexcept;

  static Element &index(pointer ptr, std::size_t nth);
};

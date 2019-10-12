#pragma once

#include "trade_ptrs_v2/synopsis.hpp"

#include "trade_core_v2/library.hpp"

//

template <class Type>
trade_ptrs_v2::Private::master_ptr<Type>::~master_ptr() noexcept {
  ptr_traits<Type>::destroy(load());
}

template <class Type>
trade_ptrs_v2::Private::master_ptr<Type>::master_ptr(pointer pointer) noexcept
    : m_pointer(pointer) {}

template <class Type>
typename trade_ptrs_v2::ptr_traits<Type>::pointer
trade_ptrs_v2::Private::master_ptr<Type>::load() const noexcept {
  return m_pointer.load(std::memory_order_relaxed);
}

template <class Type>
void trade_ptrs_v2::Private::master_ptr<Type>::store(pointer pointer) noexcept {
  m_pointer.store(pointer, std::memory_order_relaxed);
}

//

template <class Type>
typename trade_ptrs_v2::sole_ptr<Type>::pointer
    trade_ptrs_v2::sole_ptr<Type>::operator->() const noexcept {
  return this->m_pointer;
}

template <class Type>
std::add_lvalue_reference_t<
    typename trade_ptrs_v2::sole_ptr<Type>::element_type>
    trade_ptrs_v2::sole_ptr<Type>::operator*() const {
  return ptr_traits<Type>::dereference(this->m_pointer);
}

template <class Type>
typename trade_ptrs_v2::sole_ptr<Type>::element_type &
    trade_ptrs_v2::sole_ptr<Type>::operator[](std::size_t nth) const {
  return ptr_traits<Type>::index(this->m_pointer, nth);
}

template <class Type>
void trade_ptrs_v2::sole_ptr<Type>::swap(sole_ptr &that) noexcept {
  std::swap(this->m_pointer, that.m_pointer);
}

template <class Type>
trade_ptrs_v2::sole_ptr<Type> &
trade_ptrs_v2::sole_ptr<Type>::operator=(sole_ptr &&that) {
  if (this != &that) {
    this->m_pointer = that.m_pointer;
    that.m_pointer = nullptr;
  }
  return *this;
}

template <class Type>
trade_ptrs_v2::sole_ptr<Type> &
trade_ptrs_v2::sole_ptr<Type>::operator=(std::nullptr_t) {
  this->m_pointer = nullptr;
  return *this;
}

template <class Type>
bool trade_ptrs_v2::sole_ptr<Type>::operator!() const noexcept {
  return !this->m_pointer;
}

template <class Type>
trade_ptrs_v2::sole_ptr<Type>::operator bool() const noexcept {
  return !!this->m_pointer;
}

template <class Type>
typename trade_ptrs_v2::sole_ptr<Type>::pointer
trade_ptrs_v2::sole_ptr<Type>::get() const noexcept {
  return this->m_pointer;
}

//

template <class Type>
typename trade_ptrs_v2::ptr_traits<Type>::pointer
trade_core_v2::atom_traits<trade_ptrs_v2::sole_ptr<Type>>::load(
    const master_type &from) noexcept {
  return from.load();
}

template <class Type>
void trade_core_v2::atom_traits<trade_ptrs_v2::sole_ptr<Type>>::snap(
    const master_type &from, std::aligned_union_t<1, value_type> &to) {
  reinterpret_cast<value_type *>(&to)->m_pointer = from.load();
}

template <class Type>
std::optional<typename trade_core_v2::atom_traits<
    trade_ptrs_v2::sole_ptr<Type>>::master_type>
trade_core_v2::atom_traits<trade_ptrs_v2::sole_ptr<Type>>::commit(
    value_type &from, master_type &to) noexcept {
  auto p_from = from.load();
  auto p_to = to.load();
  if (p_from != p_to) {
    from.store(p_to);
    if (p_from)
      return std::make_optional(master_type(p_from));
  }
  return std::nullopt;
}

//

template <class Object>
void trade_ptrs_v2::Private::ptr_traits<Object>::destroy(pointer ptr) noexcept {
  delete ptr;
}

template <class Object>
Object *
trade_ptrs_v2::Private::ptr_traits<Object>::arrow(pointer ptr) noexcept {
  return ptr;
}

template <class Object>
std::add_lvalue_reference_t<Object>
trade_ptrs_v2::Private::ptr_traits<Object>::dereference(pointer ptr) {
  return *ptr;
}

template <class Element>
void trade_ptrs_v2::Private::ptr_traits<Element[]>::destroy(
    pointer ptr) noexcept {
  delete[] ptr;
}

template <class Element>
Element &trade_ptrs_v2::Private::ptr_traits<Element[]>::index(pointer ptr,
                                                              std::size_t nth) {
  return ptr[nth];
}

//

template <class Type>
inline void trade_ptrs_v2::ptr_traits<Type>::destroy(pointer ptr) noexcept {
  Private::ptr_traits<Type>::destroy(ptr);
}

template <class Type>
inline typename trade_ptrs_v2::ptr_traits<Type>::pointer
trade_ptrs_v2::ptr_traits<Type>::arrow(pointer ptr) noexcept {
  return Private::ptr_traits<Type>::arrow(ptr);
}

template <class Type>
inline std::add_lvalue_reference_t<
    typename trade_ptrs_v2::ptr_traits<Type>::element_type>
trade_ptrs_v2::ptr_traits<Type>::dereference(pointer ptr) {
  return Private::ptr_traits<Type>::dereference(ptr);
}

template <class Type>
inline typename trade_ptrs_v2::ptr_traits<Type>::element_type &
trade_ptrs_v2::ptr_traits<Type>::index(pointer ptr, std::size_t nth) {
  return Private::ptr_traits<Type>::index(ptr, nth);
}

//

template <class Type>
void trade_ptrs_v2::swap(trade_ptrs_v2::sole_ptr<Type> &lhs,
                         trade_ptrs_v2::sole_ptr<Type> &rhs) noexcept {
  lhs.swap(rhs);
}

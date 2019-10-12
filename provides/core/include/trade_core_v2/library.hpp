#pragma once

#include "trade_core_v2/synopsis.hpp"

//

template <template <class> class Derived,
          class Value,
          class Master,
          class Readonly>
Readonly
trade_core_v2::Private::basic_traits<Derived, Value, Master, Readonly>::load(
    const Master &from) {
  return from;
}

template <template <class> class Derived,
          class Value,
          class Master,
          class Readonly>
void trade_core_v2::Private::basic_traits<Derived, Value, Master, Readonly>::
    snap(const Master &from, std::aligned_union_t<1, Value> &to) {
  new (&to) Value(from);
}

template <template <class> class Derived,
          class Value,
          class Master,
          class Readonly>
void trade_core_v2::Private::basic_traits<Derived, Value, Master, Readonly>::
    commit(Value &from, Master &to) noexcept {
  to = from;
}

//

template <class Value>
Value trade_core_v2::Private::atomic_traits<Value>::load(
    const std::atomic<Value> &from) noexcept {
  return from.load(std::memory_order_relaxed);
}

template <class Value>
void trade_core_v2::Private::atomic_traits<Value>::snap(
    const std::atomic<Value> &from,
    std::aligned_union_t<1, Value> &to) noexcept {
  new (&to) Value(load(from));
}

template <class Value>
void trade_core_v2::Private::atomic_traits<Value>::commit(
    Value &from, std::atomic<Value> &to) noexcept {
  to.store(from, std::memory_order_relaxed);
}

//

template <class Value>
void trade_core_v2::Private::swappable_traits<Value>::commit(
    Value &from, Value &to) noexcept {
  std::swap(from, to);
}

//

template <class Value>
inline typename trade_core_v2::atom_traits<Value>::readonly_type
trade_core_v2::atom_traits<Value>::load(const master_type &from) {
  return Private::atom_traits<Value>::load(from);
}

template <class Value>
inline void trade_core_v2::atom_traits<Value>::snap(
    const master_type &from, std::aligned_union_t<1, value_type> &to) {
  Private::atom_traits<Value>::snap(from, to);
}

template <class Value>
inline void
trade_core_v2::atom_traits<Value>::commit(value_type &from,
                                          master_type &to) noexcept {
  Private::atom_traits<Value>::commit(from, to);
}

// =============================================================================

template <class Value>
template <class... Forwardable>
trade_core_v2::Private::atom<Value>::atom(Forwardable &&... value)
    : m_master(std::forward<Forwardable>(value)...) {}

//

template <class Value> trade_core_v2::atom<Value>::~atom() {}

template <class Value>
template <class... Forwardable>
trade_core_v2::atom<Value>::atom(Forwardable &&... value)
    : Private::atom<Value>(std::forward<Forwardable>(value)...) {}

template <class Value>
const typename trade_core_v2::atom<Value>::value_type &
trade_core_v2::atom<Value>::load() const {
  throw this; // TODO
}

template <class Value>
trade_core_v2::atom<Value>::operator const value_type &() const {
  return load();
}

template <class Value>
typename trade_core_v2::atom<Value>::readonly_type
trade_core_v2::atom<Value>::atomic_load() const {
  throw this; // TODO
}

template <class Value>
template <class Forwardable>
typename trade_core_v2::atom<Value>::value_type &
trade_core_v2::atom<Value>::store(Forwardable &&value) {
  throw value; // TODO
}

template <class Value>
template <class Forwardable>
typename trade_core_v2::atom<Value>::value_type &
trade_core_v2::atom<Value>::operator=(Forwardable &&value) {
  store(std::forward<Forwardable>(value));
}

template <class Value>
typename trade_core_v2::atom<Value>::value_type &
trade_core_v2::atom<Value>::ref() {
  throw this; // TODO
}

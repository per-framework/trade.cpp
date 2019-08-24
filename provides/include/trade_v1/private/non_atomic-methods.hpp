#pragma once

#include "trade_v1/private/private.hpp"

template <class Value> trade_v1::Private::non_atomic_t<Value>::non_atomic_t() {}

template <class Value>
trade_v1::Private::non_atomic_t<Value>::non_atomic_t(const Value &value)
    : m_value(value) {}

template <class Value>
void trade_v1::Private::non_atomic_t<Value>::store(const Value &value,
                                                   std::memory_order) {
  m_value = value;
}

template <class Value>
const Value &
trade_v1::Private::non_atomic_t<Value>::load(std::memory_order) const {
  return m_value;
}

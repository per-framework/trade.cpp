#pragma once

#include "trade_v1/private/private.hpp"

template <class Value> class trade_v1::Private::non_atomic_t {
  friend class Private;

  non_atomic_t();
  non_atomic_t(const Value &value);

  void store(const Value &value, std::memory_order = std::memory_order_relaxed);
  const Value &load(std::memory_order = std::memory_order_relaxed) const;

  Value m_value;
};

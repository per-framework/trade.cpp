#pragma once

#include "trade_v1/private/non_atomic.hpp"

class trade_v1::Private::atom_mono_t {
  friend class Private;
};

template <class Value> class trade_v1::Private::atom_t : Private::atom_mono_t {
  friend class Private;
  template <class> friend struct trade_v1::atom;

  static constexpr bool is_atomic = !std::is_trivially_copyable_v<Value> ||
                                    std::atomic<Value>::is_always_lock_free;

  std::conditional_t<is_atomic, std::atomic<Value>, non_atomic_t<Value>>
      m_value;

  atom_t();
  atom_t(const Value &value);
};

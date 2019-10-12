#pragma once

#include "trade_core_v2/private.hpp"

// =============================================================================

namespace trade_core_v2 {

template <class Value> struct atom : Private::atom<Value> {
  using value_type = typename atom_traits<Value>::value_type;
  using readonly_type = typename atom_traits<Value>::readonly_type;

  ~atom();

  template <class... Forwardable> atom(Forwardable &&... value);

  const value_type &load() const;
  operator const value_type &() const;

  readonly_type atomic_load() const;

  template <class Forwardable> value_type &store(Forwardable &&value);
  template <class Forwardable> value_type &operator=(Forwardable &&value);

  value_type &ref();

  atom(const atom &) = delete;
  atom &operator=(const atom &that) = delete;
};

template <class Action>
std::invoke_result_t<Action> atomically(Action &&action);

} // namespace trade_core_v2

// =============================================================================

namespace trade_core_v2 {

template <class Value> struct atom_traits {
  using value_type = typename Private::atom_traits<Value>::value_type;
  using master_type = typename Private::atom_traits<Value>::master_type;
  using readonly_type = typename Private::atom_traits<Value>::readonly_type;

  static constexpr bool is_atomic = Private::atom_traits<Value>::is_atomic;
  static constexpr bool is_trivial = Private::atom_traits<Value>::is_trivial;

  static readonly_type load(const master_type &from);

  static void snap(const master_type &from,
                   std::aligned_union_t<1, value_type> &to);

  static void commit(value_type &from, master_type &to) noexcept;
};

} // namespace trade_core_v2

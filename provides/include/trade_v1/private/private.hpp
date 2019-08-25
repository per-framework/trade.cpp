#pragma once

#include "polyfill_v1/type_traits.hpp"

#include <atomic>
#include <cstddef>

namespace trade_v1 {

template <class Value> struct atom;

template <class Config, class Action>
std::invoke_result_t<Action> atomically(Config config, Action &&action);

[[noreturn]] void retry();

/// Private implementation details.
class Private {
  template <class> friend struct atom;

  template <class Config, class Action>
  friend std::invoke_result_t<Action> atomically(Config config,
                                                 Action &&action);

  friend void retry();

  //

  struct Static;

  //

  struct backoff_t;

  template <class Value> class non_atomic_t;

  //

  class atom_mono_t;
  template <class Value> class atom_t;

  //

  struct signal_t;
  struct waiter_t;
  struct lock_t;

  using clock_t = uint64_t;
  using signed_clock_t = int64_t;
  using lock_ix_t = int16_t;

  static constexpr lock_ix_t n_locks = 251;
  static lock_t s_locks[n_locks];

  static std::atomic<clock_t> s_clock;

  //

  using state_t = uint8_t;
  static constexpr state_t INITIAL = 0, READ = 1, WRITTEN = 2;

  struct access_base_t;

  template <class Value,
            bool is_trivially_destructible =
                std::is_trivially_destructible_v<Value>>
  struct access_t;

  using destroy_t = void (*)(clock_t t, access_base_t *self);

  template <class Value> static void destroy(clock_t t, access_base_t *access);

  //

  struct transaction_base_t;
  struct transaction_heap_t;
  template <class Config> struct transaction_stack_t;

  thread_local static transaction_base_t *s_transaction;

  //

  template <class Transaction, class Result> struct run_t;

  static lock_ix_t lock_ix_of(const atom_mono_t *atom);

  static void signal(waiter_t *work);

  static access_base_t *insert(transaction_base_t *transaction,
                               atom_mono_t *atom,
                               size_t align_m1,
                               size_t size);

  template <class Value>
  static access_t<Value> *insert(transaction_base_t *transaction,
                                 atom_t<Value> *atom);

  static void destroy(transaction_base_t *transaction);

  static bool try_commit(transaction_base_t *transaction);

  //

  template <class Value> static Value load(const atom_t<Value> &atom);

  template <class Value> static Value unsafe_load(const atom_t<Value> &atom);

  template <class Value, class Forwardable>
  static Value &store(atom_t<Value> &atom, Forwardable &&value);

  template <class Value> static Value &ref(atom_t<Value> &atom);

  template <class Config, class Action>
  static std::invoke_result_t<Action> atomically(Config config,
                                                 Action &&action);

  [[noreturn]] static void retry(transaction_base_t *transaction);
};

} // namespace trade_v1

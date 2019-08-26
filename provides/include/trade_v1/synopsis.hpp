#pragma once

#include "trade_v1/private/atom.hpp"

/// A transactional locking library.
namespace trade_v1 {

/// Type of transactional variables or atoms.
template <class Value> struct atom : Private::atom_t<Value> {
  /// Type of contained value.
  using value_type = Value;

  /// Constructs an atom initialized with default constructed value.
  atom();

  /// Constructs an atom initialized with the given value.
  atom(const Value &value);

  /// Atoms are not CopyConstructible.
  atom(const atom &) = delete;

  /// Loads the current value of the atom within a transaction.  Implicit
  /// conversion of `atom` is equivalent to `atom.load()`.
  operator Value() const;

  /// Loads the current value of the atom within a transaction.  Implicit
  /// conversion of `atom` is equivalent to `atom.load()`.
  Value load() const;

  /// Atomically loads the current value of the atom outside of any transaction.
  Value unsafe_load() const;

  /// Stores the given value to the given atom within a transaction.  `atom =
  /// value` is equivalent to `atom.store(value)`.
  template <class Forwardable> Value &operator=(Forwardable &&value);

  /// Atoms are not CopyAssignable.
  atom &operator=(const atom &that) = delete;

  /// Stores the given value to the given atom within a transaction.  `atom =
  /// value` is equivalent to `atom.store(value)`.
  template <class Forwardable> Value &store(Forwardable &&value);

  /// Returns a mutable reference to the current value of the atom within a
  /// transaction.  `atom.ref()` is roughly equivalent to
  /// `atom.store(atom.load())`, but accesses the transaction log only once.
  Value &ref();

  // Atoms are no larger than atomic values.
  static_assert(sizeof(Private::atom_t<Value>) <= sizeof(std::atomic<Value>));
};

/// Invokes the given action atomically with respect to other transactions.  Any
/// direct side-effects within the action may be performed multiple times.
/// `atomically(action)` is equivalent to `atomically(stack<1024>, action)`.
template <class Action>
std::invoke_result_t<Action> atomically(Action &&action);

/// Specifies heap allocation and initial heap size for transaction log to
/// `atomically`.
enum heap : size_t {
  /// Specifies that the transaction is to be assumed read-only.  Read-only
  /// transactions can be performed more efficiently, because they do not
  /// require constructing a transaction log.  Any write access during the
  /// transaction will cause the transaction to restart in heap allocation mode.
  /// `assume_readonly` is equivalent to `heap(0)`.
  assume_readonly
};

/// Type for specifying stack allocation size configuration to `atomically`.
template <size_t fixed_size> struct stack_t {};

/// Specifies stack allocation and fixed maximum size of transaction log to
/// `atomically`.
template <size_t fixed_size>
[[maybe_unused]] constexpr stack_t<fixed_size> stack = {};

/// Invokes the given action atomically with respect to other transactions.  Any
/// direct side-effects within the action may be performed multiple times.
/// `atomically(action)` is equivalent to `atomically(stack<1024>, action)`.
template <class Config, class Action>
std::invoke_result_t<Action> atomically(Config config, Action &&action);

/// Aborts the current transaction, possibly blocks waiting for changes to any
/// atoms read during the transaction, and then restarts the transaction.
[[noreturn]] void retry();

} // namespace trade_v1

#pragma once

#include "trade_v1/config.hpp"
#include "trade_v1/private/access-methods.hpp"
#include "trade_v1/private/lock.hpp"
#include "trade_v1/private/meta.hpp"
#include "trade_v1/private/transaction-methods.hpp"

#include "molecular_v1/backoff.hpp"

#include <utility>

inline trade_v1::Private::lock_ix_t
trade_v1::Private::lock_ix_of(const atom_mono_t *atom) {
  return static_cast<lock_ix_t>(reinterpret_cast<size_t>(atom) % n_locks);
}

template <class Value>
void trade_v1::Private::destroy(clock_t t, access_base_t *access_base) {
  auto access = static_cast<access_t<Value> *>(access_base);
  if (t) {
    auto atom = static_cast<atom_t<Value> *>(access->m_atom);
    atom->m_value.store(access->m_current, std::memory_order_relaxed);
    auto ix = access->m_lock_ix;
    if (0 <= ix) {
      auto &lock = s_locks[ix];
      if (auto first = lock.m_first)
        signal(first);
      lock.m_clock.store(t, std::memory_order_release);
    }
  } else {
    access->destroy();
  }
}

template <class Value>
trade_v1::Private::access_t<Value> *
trade_v1::Private::insert(transaction_base_t *transaction,
                          atom_t<Value> *atom) {
  return static_cast<access_t<Value> *>(
      insert(transaction, atom, meta<Value>::s_instance));
}

template <class Value>
Value trade_v1::Private::load(const atom_t<Value> &atom) {
  auto transaction = s_transaction;
  if (transaction->m_alloc) {
    auto access = insert(transaction, const_cast<atom_t<Value> *>(&atom));
    if (access->m_state == INITIAL) {
      auto &lock = s_locks[access->m_lock_ix];
      auto s = lock.m_clock.load();
      if (transaction->m_start < s)
        throw transaction;
      new (&access->m_current) Value(atom.m_value.load());
      access->m_state = READ;
      if (s != lock.m_clock.load())
        throw transaction;
    }
    return access->m_current;
  } else {
    auto &lock = s_locks[lock_ix_of(&atom)];
    auto s = lock.m_clock.load();
    if (transaction->m_start < s)
      throw transaction;
    Value result = atom.m_value.load();
    if (s != lock.m_clock.load())
      throw transaction;
    return result;
  }
}

template <class Value>
Value trade_v1::Private::unsafe_load(const atom_t<Value> &atom) {
  if (Private::atom_t<Value>::is_atomic) {
    return atom.m_value.load(std::memory_order_relaxed);
  } else {
    auto &lock = s_locks[lock_ix_of(&atom)];
    molecular::backoff backoff;
    while (true) {
      auto s = lock.m_clock.load();
      if (0 <= static_cast<signed_clock_t>(s)) {
        Value result = atom.m_value.load();
        if (s == lock.m_clock.load())
          return result;
      }
      backoff();
    }
  }
}

template <class Value, class Forwardable>
Value &trade_v1::Private::store(atom_t<Value> &atom, Forwardable &&value) {
  auto access = insert(s_transaction, &atom);
  switch (access->m_state) {
  case INITIAL:
    new (&access->m_current) Value(std::forward<Forwardable>(value));
    access->m_state = WRITTEN;
    break;
  case READ:
    access->retain_move();
    access->m_state = READ + WRITTEN;
    [[fallthrough]];
  default:
    access->m_current = std::forward<Forwardable>(value);
  }
  return access->m_current;
}

template <class Value> Value &trade_v1::Private::ref(atom_t<Value> &atom) {
  auto transaction = s_transaction;
  auto access = insert(transaction, &atom);
  switch (access->m_state) {
  case INITIAL: {
    auto &lock = s_locks[access->m_lock_ix];
    auto s = lock.m_clock.load();
    if (transaction->m_start < s)
      throw transaction;
    new (&access->m_current) Value(atom.m_value.load());
    access->m_state = READ;
    if (s != lock.m_clock.load())
      throw transaction;
    [[fallthrough]];
  }
  case READ:
    access->retain_copy();
    access->m_state = READ + WRITTEN;
  }
  return access->m_current;
}

template <class Config, class Action>
std::invoke_result_t<Action> trade_v1::Private::atomically(Config config,
                                                           Action &&action) {
  return s_transaction
             ? std::forward<Action>(action)()
             : run_t<std::conditional_t<std::is_same_v<Config, heap>,
                                        transaction_heap_t,
                                        transaction_stack_t<Config>>,
                     std::invoke_result_t<Action>>::run(config,
                                                        std::forward<Action>(
                                                            action));
}

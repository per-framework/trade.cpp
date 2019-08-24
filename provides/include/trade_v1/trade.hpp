#pragma once

#include "trade_v1/synopsis.hpp"

#include "trade_v1/config.hpp"

#include "dumpster_v1/finally.hpp"

#include "intrinsics_v1/pause.hpp"

#include <cstdint>
#include <memory>
#include <utility>

//

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

//

struct trade_v1::Private::access_base_t {
  ~access_base_t() = delete;

  access_base_t *m_children[2];
  atom_mono_t *m_atom;
  state_t m_state;
  lock_ix_t m_lock_ix;
  destroy_t m_destroy;
};

template <class Value>
struct trade_v1::Private::access_t<Value, false> : access_base_t {
  ~access_t() = delete;

  Value m_current;
  Value m_original;

  void retain() { new (&m_original) Value(std::move(m_current)); }
  void destroy() {
    if (INITIAL != m_state) {
      m_current.~Value();
      if (READ + WRITTEN == m_state)
        m_original.~Value();
    }
  }
};

template <class Value>
struct trade_v1::Private::access_t<Value, true> : access_base_t {
  ~access_t() = delete;

  Value m_current;

  void retain() {}
  void destroy() {}
};

//

struct trade_v1::Private::transaction_base_t {
  ~transaction_base_t() { s_transaction = nullptr; }

  transaction_base_t() { s_transaction = this; }

  clock_t m_start;
  access_base_t *m_accesses;
  uint8_t *m_alloc;
  uint8_t *m_limit;
};

struct trade_v1::Private::transaction_heap_t : transaction_base_t {
  transaction_heap_t(heap initial_size)
      : m_block(initial_size ? new uint8_t[initial_size] : nullptr) {
    m_alloc = m_limit = m_block.get() + initial_size;
  }

  void start() {
    if (m_limit < m_alloc) {
      size_t size = (m_limit - m_block.get()) * 2;
      if (size < 8 * sizeof(access_t<size_t>))
        size = 8 * sizeof(access_t<size_t>);
      m_block.reset(new uint8_t[size]);
      m_limit = m_block.get() + size;
    }

    m_accesses = nullptr;
    m_alloc = m_block.get();
    m_start = s_clock;
  }

  std::unique_ptr<uint8_t[]> m_block;
};

template <size_t Bytes>
struct trade_v1::Private::transaction_stack_t<trade_v1::stack_t<Bytes>>
    : transaction_base_t {
  transaction_stack_t(trade_v1::stack_t<Bytes>) {
    m_alloc = m_limit = m_space + sizeof(m_space);
  }

  void start() {
    if (m_limit < m_alloc)
      throw std::bad_alloc();
    m_accesses = nullptr;
    m_alloc = m_space;
    m_start = s_clock;
  }

  alignas(alignof(access_base_t)) uint8_t m_space[Bytes];
};

//

struct trade_v1::Private::waiter_t {
  waiter_t *m_next;
  waiter_t **m_link;
  signal_t *m_signal;
};

struct trade_v1::Private::lock_t {
  std::atomic<clock_t> m_clock;
  size_t m_count;
  std::atomic<transaction_base_t *> m_owner;
  waiter_t *m_first;
};

inline trade_v1::Private::lock_ix_t
trade_v1::Private::lock_ix_of(const atom_mono_t *atom) {
  return static_cast<lock_ix_t>(reinterpret_cast<size_t>(atom) % n_locks);
}

//

template <class Value>
void trade_v1::Private::destroy(clock_t t, access_base_t *access_base) {
  auto access = static_cast<access_t<Value> *>(access_base);
  if (t) {
    auto atom = static_cast<atom_t<Value> *>(access->m_atom);
    atom->m_value.store(access->m_current, std::memory_order_relaxed);
    auto &lock = s_locks[access->m_lock_ix];

    auto count = lock.m_count;
    if (count) {
      lock.m_count = count - 1;
    } else {
      if (auto first = lock.m_first)
        signal(first);

      lock.m_owner.store(nullptr, std::memory_order_relaxed);
      lock.m_clock.store(t, std::memory_order_release);
    }
  }
  access->destroy();
}

//

template <class Value>
trade_v1::Private::access_t<Value> *
trade_v1::Private::insert(transaction_base_t *transaction,
                          atom_t<Value> *atom) {
  return static_cast<access_t<Value> *>(insert(transaction,
                                               atom,
                                               alignof(access_t<Value>) - 1,
                                               sizeof(access_t<Value>)));
}

//

template <class Value> trade_v1::Private::atom_t<Value>::atom_t() {}

template <class Value>
trade_v1::Private::atom_t<Value>::atom_t(const Value &value) : m_value(value) {}

template <class Value> trade_v1::atom<Value>::atom() {}

template <class Value>
trade_v1::atom<Value>::atom(const Value &value)
    : Private::atom_t<Value>(value) {}

template <class Value> trade_v1::atom<Value>::operator Value() const {
  return Private::load(*this);
}

template <class Value> Value trade_v1::atom<Value>::load() const {
  return Private::load(*this);
}

template <class Value> Value trade_v1::atom<Value>::unsafe_load() const {
  return Private::unsafe_load(*this);
}

template <class Value>
template <class Forwardable>
Value &trade_v1::atom<Value>::operator=(Forwardable &&value) {
  return store(std::forward<Forwardable>(value));
}

template <class Value>
template <class Forwardable>
Value &trade_v1::atom<Value>::store(Forwardable &&value) {
  return Private::store(*this, std::forward<Forwardable>(value));
}

template <class Value>
Value trade_v1::Private::load(const atom_t<Value> &atom) {
  auto transaction = s_transaction;
  if (transaction->m_alloc) {
    auto access = insert(transaction, const_cast<atom_t<Value> *>(&atom));
    if (access->m_state == INITIAL) {
      access->m_destroy = destroy<Value>;
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
    while (true) {
      auto s = lock.m_clock.load();
      if (s < ~s) {
        Value result = atom.m_value.load();
        if (s == lock.m_clock.load())
          return result;
      }
      intrinsics::pause();
    }
  }
}

template <class Value, class Forwardable>
Value &trade_v1::Private::store(atom_t<Value> &atom, Forwardable &&value) {
  auto access = insert(s_transaction, &atom);
  switch (access->m_state) {
  case INITIAL:
    new (&access->m_current) Value(std::forward<Forwardable>(value));
    access->m_destroy = destroy<Value>;
    access->m_state = WRITTEN;
    break;
  case READ:
    access->retain();
    access->m_state = READ + WRITTEN;
    [[fallthrough]];
  default:
    access->m_current = std::forward<Forwardable>(value);
  }
  return access->m_current;
}

template <class Transaction, class Result> struct trade_v1::Private::run_t {
  template <class Config, class Action>
  static Result run(Config config, Action &&action) {
    Transaction transaction(config);
    while (true) {
      try {
        transaction.start();
        auto destroy_accesses =
            dumpster::finally([&]() { destroy(&transaction); });
        Result result = action();
        if (try_commit(&transaction))
          return result;
      } catch (transaction_base_t *) {
      }
    }
  }
};

template <class Transaction>
struct trade_v1::Private::run_t<Transaction, void> {
  template <class Config, class Action>
  static void run(Config config, Action &&action) {
    Transaction transaction(config);
    while (true) {
      try {
        transaction.start();
        auto destroy_accesses =
            dumpster::finally([&]() { destroy(&transaction); });
        action();
        if (try_commit(&transaction))
          return;
      } catch (transaction_base_t *) {
      }
    }
  }
};

template <class Config, class Action>
std::invoke_result_t<Action> trade_v1::atomically(Config config,
                                                  Action &&action) {
  return Private::s_transaction
             ? action()
             : Private::run_t<
                   std::conditional_t<std::is_same_v<Config, heap>,
                                      Private::transaction_heap_t,
                                      Private::transaction_stack_t<Config>>,
                   std::invoke_result_t<Action>>::run(config, action);
}

template <class Action>
std::invoke_result_t<Action> trade_v1::atomically(Action &&action) {
  return atomically(stack<1024>, action);
}

inline void trade_v1::retry() { Private::retry(Private::s_transaction); }

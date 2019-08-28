#include "trade_v1/trade.hpp"

#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>

struct trade_v1::Private::waiter_t {
  waiter_t *m_next;
  waiter_t **m_link;
  signal_t *m_signal;
};

thread_local uint32_t trade_v1::Private::backoff_t::s_seed;

trade_v1::Private::lock_t trade_v1::Private::s_locks[n_locks];

thread_local trade_v1::Private::transaction_base_t
    *trade_v1::Private::s_transaction;

std::atomic<trade_v1::Private::clock_t> trade_v1::Private::s_clock(0);

struct trade_v1::Private::signal_t {
  signal_t() : m_signaled(false) {}
  std::mutex m_mutex;
  std::atomic<bool> m_signaled;
  std::condition_variable m_condition_variable;
};

struct trade_v1::Private::Static {
  static clock_t acquire(lock_t &lock) {
    backoff_t backoff;
    while (true) {
      auto u = lock.m_clock.load(std::memory_order_relaxed);
      if (0 <= static_cast<signed_clock_t>(u))
        if (lock.m_clock.compare_exchange_weak(
                u, ~u, std::memory_order_acquire))
          return u;
      backoff();
    }
  }

  static void release(lock_t &lock, clock_t at) {
    lock.m_clock.store(at, std::memory_order_release);
  }

  static void release(lock_t &lock) {
    release(lock, ~lock.m_clock.load(std::memory_order_relaxed));
  }

  static void *align_to(size_t align_m1, void *ptr) {
    return reinterpret_cast<void *>((reinterpret_cast<size_t>(ptr) + align_m1) &
                                    ~align_m1);
  }

  static access_base_t *alloc_align(transaction_base_t *transaction,
                                    size_t align_m1) {
    return static_cast<access_base_t *>(
        align_to(align_m1, transaction->m_alloc));
  }

  static bool
  alloc_limit(transaction_base_t *transaction, void *start, size_t size) {
    return (transaction->m_alloc = static_cast<uint8_t *>(start) + size) <=
           transaction->m_limit;
  }

  static void wait(clock_t t, signal_t &signal, access_base_t *root) {
    if (root) {
      if (root->m_state & READ) {
        auto &lock = s_locks[root->m_lock_ix];

        auto s = lock.m_clock.load(std::memory_order_relaxed);
        if (t < s || !lock.m_clock.compare_exchange_strong(
                         s, ~s, std::memory_order_acquire))
          return;

        auto first = lock.m_first;

        waiter_t waiter = {first, &lock.m_first, &signal};

        lock.m_first = &waiter;
        if (first)
          first->m_link = &waiter.m_next;

        release(lock, s);

        wait(t, signal, root->m_children[1]);

        auto u = acquire(lock);

        if (auto next = *waiter.m_link = waiter.m_next)
          next->m_link = waiter.m_link;

        release(lock, u);
      } else {
        wait(t, signal, root->m_children[1]);
      }
    } else {
      std::unique_lock<std::mutex> guard(signal.m_mutex);
      if (!signal.m_signaled.load(std::memory_order_relaxed))
        signal.m_condition_variable.wait(guard);
    }
  }

  template <class Action>
  static void destructively_in_order(access_base_t *root, Action &&action) {
    access_base_t *prev = nullptr;

    while (root) {
      auto left = root->m_children[0];
      if (left) {
        root->m_children[0] = prev;
        prev = root;
        root = left;
      } else {
        action(root);

        root = root->m_children[1];
        if (!root) {
          root = prev;
          if (prev) {
            prev = prev->m_children[0];
            root->m_children[0] = nullptr;
          }
        }
      }
    }
  }

  static void append_to(access_base_t **last, access_base_t *node) {
    (*last)->m_children[1] = node;
    *last = node;
  }

  static void append_to(access_base_t ***tail, access_base_t *node) {
    **tail = node;
    *tail = &node->m_children[1];
  }

  static void unlock_and_destroy(access_base_t *it) {
    while (it) {
      auto ix = it->m_lock_ix;
      if (0 <= ix) {
        auto &lock = s_locks[ix];
        Static::release(lock);
      }
      it->m_destroy(0, it);

      it = it->m_children[1];
    }
  }
};

trade_v1::Private::access_base_t *
trade_v1::Private::insert(transaction_base_t *transaction,
                          atom_mono_t *access_atom,
                          size_t align_m1,
                          size_t size) {
  auto root = transaction->m_accesses;

  auto access_ix = lock_ix_of(access_atom);

  if (!root) {
    auto access = Static::alloc_align(transaction, align_m1);
    if (Static::alloc_limit(transaction, access, size)) {
      access->m_children[0] = nullptr;
      access->m_children[1] = nullptr;
      access->m_atom = access_atom;
      access->m_state = INITIAL;
      access->m_lock_ix = access_ix;
      return transaction->m_accesses = access;
    } else {
      throw transaction;
    }
  }

  if (access_atom == root->m_atom)
    return root;

  access_base_t *side_root[2];
  access_base_t **side_near[2] = {&side_root[0], &side_root[1]};

  while (true) {
    if (access_ix <= root->m_lock_ix &&
        (access_ix != root->m_lock_ix || access_atom < root->m_atom)) {
      auto next = root->m_children[0];

      if (!next) {
        *side_near[0] = nullptr;
        *side_near[1] = root->m_children[1];
        root->m_children[1] = side_root[1];
        auto access = Static::alloc_align(transaction, align_m1);
        if (Static::alloc_limit(transaction, access, size)) {
          access->m_children[0] = side_root[0];
          access->m_children[1] = root;
          access->m_atom = access_atom;
          access->m_state = INITIAL;
          access->m_lock_ix = access_ix;
          return transaction->m_accesses = access;
        } else {
          root->m_children[0] = side_root[0];
          transaction->m_accesses = root;
          throw transaction;
        }
      }

      if (access_ix <= next->m_lock_ix &&
          (access_ix != next->m_lock_ix || access_atom < next->m_atom)) {
        root->m_children[0] = next->m_children[1];
        next->m_children[1] = root;
        root = next;
        next = root->m_children[0];

        if (!next) {
          *side_near[0] = nullptr;
          *side_near[1] = root;
          auto access = Static::alloc_align(transaction, align_m1);
          if (Static::alloc_limit(transaction, access, size)) {
            access->m_children[0] = side_root[0];
            access->m_children[1] = side_root[1];
            access->m_atom = access_atom;
            access->m_state = INITIAL;
            access->m_lock_ix = access_ix;
            return transaction->m_accesses = access;
          } else {
            root->m_children[0] = side_root[0];
            transaction->m_accesses = side_root[1];
            throw transaction;
          }
        }
      }

      *side_near[1] = root;
      side_near[1] = &root->m_children[0];
      root = next;
    } else {
      auto next = root->m_children[1];

      if (!next) {
        *side_near[0] = root->m_children[0];
        *side_near[1] = nullptr;
        root->m_children[0] = side_root[0];
        auto access = Static::alloc_align(transaction, align_m1);
        if (Static::alloc_limit(transaction, access, size)) {
          access->m_children[0] = root;
          access->m_children[1] = side_root[1];
          access->m_atom = access_atom;
          access->m_state = INITIAL;
          access->m_lock_ix = access_ix;
          return transaction->m_accesses = access;
        } else {
          root->m_children[1] = side_root[1];
          transaction->m_accesses = root;
          throw transaction;
        }
      }

      if (access_ix >= next->m_lock_ix &&
          (access_ix != next->m_lock_ix || access_atom > next->m_atom)) {
        root->m_children[1] = next->m_children[0];
        next->m_children[0] = root;
        root = next;
        next = root->m_children[1];

        if (!next) {
          *side_near[0] = root;
          *side_near[1] = nullptr;
          auto access = Static::alloc_align(transaction, align_m1);
          if (Static::alloc_limit(transaction, access, size)) {
            access->m_children[0] = side_root[0];
            access->m_children[1] = side_root[1];
            access->m_atom = access_atom;
            access->m_state = INITIAL;
            access->m_lock_ix = access_ix;
            return transaction->m_accesses = access;
          } else {
            root->m_children[1] = side_root[1];
            transaction->m_accesses = side_root[0];
            throw transaction;
          }
        }
      }

      *side_near[0] = root;
      side_near[0] = &root->m_children[1];
      root = next;
    }

    if (access_atom == root->m_atom) {
      transaction->m_accesses = root;

      *side_near[0] = root->m_children[0];
      *side_near[1] = root->m_children[1];
      root->m_children[0] = side_root[0];
      root->m_children[1] = side_root[1];

      return root;
    }
  }
}

void trade_v1::Private::signal(waiter_t *work) {
  do {
    auto signal = work->m_signal;
    if (!signal->m_signaled.load(std::memory_order_relaxed)) {
      {
        std::unique_lock<std::mutex> guard(signal->m_mutex);
        signal->m_signaled.store(true, std::memory_order_relaxed);
      }
      signal->m_condition_variable.notify_all();
    }
    work = work->m_next;
  } while (work);
}

void trade_v1::Private::retry(transaction_base_t *transaction) {
  {
    access_base_t **tail = &transaction->m_accesses;

    Static::destructively_in_order(
        *tail, [&](auto node) { Static::append_to(&tail, node); });
  }

  if (auto root = transaction->m_accesses) {
    signal_t signal;
    Static::wait(transaction->m_start, signal, root);
  } else {
    auto limit = transaction->m_limit;
    if (!limit)
      transaction->m_alloc = limit + 1;
  }

  throw transaction;
}

void trade_v1::Private::destroy(transaction_base_t *transaction) {
  Static::destructively_in_order(transaction->m_accesses,
                                 [](auto node) { node->m_destroy(0, node); });
}

bool trade_v1::Private::try_commit(transaction_base_t *transaction) {
  auto t = transaction->m_start;

  access_base_t writes;
  writes.m_lock_ix = -1;

  {
    access_base_t *root = transaction->m_accesses;

    access_base_t *writes_last = &writes;
    access_base_t **reads_tail = &transaction->m_accesses;

    Static::destructively_in_order(root, [&](auto node) {
      if (writes_last && WRITTEN <= node->m_state) {
        auto lock_ix = node->m_lock_ix;
        if (lock_ix == writes_last->m_lock_ix) {
          writes_last->m_lock_ix = -1;
          Static::append_to(&writes_last, node);
        } else {
          auto &lock = s_locks[node->m_lock_ix];
          auto s = lock.m_clock.load(std::memory_order_relaxed);
          if (t < s || !lock.m_clock.compare_exchange_strong(
                           s, ~s, std::memory_order_acquire)) {
            Static::append_to(&reads_tail, node);
            writes_last = writes_last->m_children[1] = nullptr;
            Static::unlock_and_destroy(writes.m_children[1]);
          } else {
            Static::append_to(&writes_last, node);
          }
        }
      } else {
        Static::append_to(&reads_tail, node);
      }
    });

    *reads_tail = nullptr;

    if (!writes_last)
      return false;

    writes_last->m_children[1] = nullptr;
  }

  auto u = s_clock++;

  if (u != t) {
    auto wr = writes.m_children[1];
    for (auto it = transaction->m_accesses; it; it = it->m_children[1]) {
      auto ix = it->m_lock_ix;
      auto &lock = s_locks[ix];
      auto s = lock.m_clock.load(std::memory_order_relaxed);
      if (s <= t)
        continue;
      if (static_cast<signed_clock_t>(s) < 0) {
        while (wr) {
          if (wr->m_lock_ix < ix) {
            wr = wr->m_children[1];
          } else if (wr->m_lock_ix == ix) {
            break;
          } else {
            Static::unlock_and_destroy(writes.m_children[1]);
            return false;
          }
        }
      }
    }
  }

  u += 1;
  for (auto it = writes.m_children[1]; it; it = it->m_children[1])
    it->m_destroy(u, it);

  return true;
}

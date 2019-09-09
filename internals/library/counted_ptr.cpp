#include "testing/counted_ptr.hpp"

#include "molecular_v1/backoff.hpp"

testing::Private::lock_t testing::Private::s_locks[testing::Private::n_locks];

testing::Private::lock_t *testing::Private::lock_of(void *ptr) {
  return &s_locks[reinterpret_cast<size_t>(ptr) % n_locks];
}

bool testing::Private::try_acquire(lock_t *lock,
                                   const std::atomic<void *> *at,
                                   void *pointer,
                                   count_t **first_out) {
  auto locked = reinterpret_cast<count_t *>(1);
  auto first = lock->m_first.load(std::memory_order_relaxed);
  if (first != locked && lock->m_first.compare_exchange_weak(first, locked)) {
    if (pointer == at->load()) {
      *first_out = first;
      return true;
    }
    lock->m_first.store(first, std::memory_order_relaxed);
  }
  return false;
}

void testing::Private::release(lock_t *lock, count_t *first) {
  lock->m_first.store(first, std::memory_order_release);
}

void *testing::Private::destroy(std::atomic<void *> *to) {
  void *to_pointer;
  lock_t *to_lock;
  count_t *to_first;

  molecular_v1::backoff backoff;
  while (true) {
    to_pointer = to->load();
    if (!to_pointer)
      return nullptr;
    to_lock = lock_of(to_pointer);
    if (try_acquire(to_lock, to, to_pointer, &to_first))
      break;
    backoff();
  }

  count_t *to_count;
  auto prev = &to_first;
  while (true) {
    to_count = *prev;
    if (to_count->m_object == to_pointer) {
      if (!--(to_count->m_count)) {
        *prev = to_count->m_next;
      } else {
        to_count = nullptr;
        to_pointer = nullptr;
      }
      break;
    } else {
      prev = &to_count->m_next;
    }
  }

  release(to_lock, to_first);

  delete to_count;

  return to_pointer;
}

void *testing::Private::destroy_and_set(std::atomic<void *> *to, void *after) {
  void *to_pointer;
  lock_t *to_lock;
  count_t *to_first;

  molecular_v1::backoff backoff;
  while (true) {
    to_pointer = to->load();
    if (!to_pointer &&
        (nullptr == after || to->compare_exchange_strong(to_pointer, after)))
      return nullptr;
    to_lock = lock_of(to_pointer);
    if (try_acquire(to_lock, to, to_pointer, &to_first))
      break;
    backoff();
  }

  count_t *to_count;
  auto prev = &to_first;
  while (true) {
    to_count = *prev;
    if (to_count->m_object == to_pointer) {
      if (!--(to_count->m_count)) {
        *prev = to_count->m_next;
      } else {
        to_count = nullptr;
        to_pointer = nullptr;
      }
      break;
    } else {
      prev = &to_count->m_next;
    }
  }

  to->store(after, std::memory_order_relaxed);

  release(to_lock, to_first);

  delete to_count;

  return to_pointer;
}

void testing::Private::create_from_non_null(void *from_pointer) {
  count_t *from_count = new count_t;
  from_count->m_object = from_pointer;
  from_count->m_count = 1;

  std::atomic<void *> from_atomic(from_pointer);
  auto from_lock = lock_of(from_pointer);

  molecular_v1::backoff backoff;
  while (
      !try_acquire(from_lock, &from_atomic, from_pointer, &from_count->m_next))
    backoff();

  release(from_lock, from_count);
}

void *testing::Private::copy_from(const std::atomic<void *> *from) {
  void *from_pointer;
  lock_t *from_lock;
  count_t *from_first;

  molecular_v1::backoff backoff;
  while (true) {
    from_pointer = from->load();
    if (!from_pointer)
      return nullptr;
    from_lock = lock_of(from_pointer);
    if (try_acquire(from_lock, from, from_pointer, &from_first))
      break;
    backoff();
  }

  auto count = from_first;
  while (true) {
    if (count->m_object == from_pointer) {
      ++(count->m_count);
      break;
    } else {
      count = count->m_next;
    }
  }

  release(from_lock, from_first);

  return from_pointer;
}

void *testing::Private::move_to(std::atomic<void *> *to,
                                std::atomic<void *> *from) {
  return destroy_and_set(to, copy_from(from));
}

#pragma once

#include <atomic>
#include <cstddef>

#include "testing/config.hpp"

namespace testing {

template <class T> class counted_ptr;

class Private {
  template <class> friend class counted_ptr;

  struct count_t {
    count_t *m_next;
    const void *m_object;
    size_t m_count;
  };

  struct lock_t {
    std::atomic<count_t *> m_first;
  };

  static constexpr uint32_t n_locks = 131071;
  static lock_t s_locks[n_locks];

  static lock_t *lock_of(void *ptr);

  static bool try_acquire(lock_t *lock,
                          const std::atomic<void *> *at,
                          void *pointer,
                          count_t **first);

  static void release(lock_t *lock, count_t *first);

  static void *destroy(std::atomic<void *> *to);
  static void *destroy_and_set(std::atomic<void *> *to, void *after);

  static void create_from_non_null(void *from);
  static void *copy_from(const std::atomic<void *> *from);

  static void *move_to(std::atomic<void *> *to, std::atomic<void *> *from);
};

template <class T> class counted_ptr : Private {
  std::atomic<void *> m_pointer;

public:
  ~counted_ptr() { delete static_cast<T *>(Private::destroy(&m_pointer)); }

  counted_ptr() : m_pointer(nullptr) {}
  counted_ptr(std::nullptr_t) : m_pointer(nullptr) {}

  explicit counted_ptr(T *pointer) : m_pointer(pointer) {
    if (pointer)
      Private::create_from_non_null(pointer);
  }

  counted_ptr(const counted_ptr &that)
      : m_pointer(static_cast<T *>(Private::copy_from(&that.m_pointer))) {}

  counted_ptr(counted_ptr &&that) : m_pointer(nullptr) {
    Private::move_to(&m_pointer, &that.m_pointer);
  }

  counted_ptr &operator=(std::nullptr_t) {
    delete static_cast<T *>(Private::destroy_and_set(&m_pointer, nullptr));
    return *this;
  }

  counted_ptr &operator=(const counted_ptr &that) {
    if (this != &that)
      delete static_cast<T *>(
          Private::destroy_and_set(&m_pointer, copy_from(&that.m_pointer)));
    return *this;
  }

  counted_ptr &operator=(counted_ptr &&that) {
    if (this != &that)
      delete static_cast<T *>(Private::move_to(&m_pointer, &that.m_pointer));
    return *this;
  }

  void reset(T *pointer) {
    if (pointer)
      create_from_non_null(pointer);
    delete static_cast<T *>(destroy_and_set(&m_pointer, pointer));
  }

  T *get() const {
    return static_cast<T *>(m_pointer.load(std::memory_order_relaxed));
  }

  T *operator->() const { return get(); }

  bool operator!() const { return !get(); }

  operator bool() const { return !!get(); }
}; // namespace testing

} // namespace testing

namespace std {

template <class Value> class atomic<testing::counted_ptr<Value>> {
  using T = testing::counted_ptr<Value>;
  T m_ptr;

public:
  atomic() {}
  atomic(std::nullptr_t) {}

  atomic(const T &ptr) : m_ptr(ptr) {}

  atomic(const atomic &) = delete;

  static constexpr bool is_always_lock_free = false;

  bool is_lock_free() const { return false; }

  T load() const { return m_ptr; }
  T load(memory_order) const { return m_ptr; }

  operator T() const { return m_ptr; }

  void store(nullptr_t) { m_ptr = nullptr; }
  void store(const T &desired) { m_ptr = desired; }

  void store(nullptr_t, memory_order) { m_ptr = nullptr; }
  void store(const T &desired, memory_order) { m_ptr = desired; }

  T operator=(nullptr_t) { return m_ptr = nullptr; }
  T operator=(const T &desired) { return m_ptr = desired; }

  atomic &operator=(const atomic &) = delete;
};

} // namespace std

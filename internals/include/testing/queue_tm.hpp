#pragma once

#include "trade_v1/trade.hpp"

#include "polyfill_v1/memory.hpp"
#include <optional>
#include <vector>

#include "testing/counted_ptr.hpp"

namespace testing {

/// A transactional queue for testing purposes.
template <class Value> class queue_tm {
  template <class T> using ptr_t = counted_ptr<T>;

  struct node_t {
    trade::atom<ptr_t<node_t>> m_next;
    Value m_value;
#ifndef NDEBUG
    ~node_t() { --s_live_nodes; }
#endif
    template <class ForwardableValue>
    node_t(ForwardableValue &&value)
        : m_value(std::forward<ForwardableValue>(value)) {
#ifndef NDEBUG
      ++s_live_nodes;
#endif
    }
  };

  trade::atom<ptr_t<node_t>> m_first;
  trade::atom<ptr_t<node_t>> m_last;

public:
  using value_t = Value;

  queue_tm() = default;

  queue_tm(const queue_tm &) = delete;
  queue_tm &operator=(const queue_tm &) = delete;

  size_t size() const;

  bool empty() const;

  void clear();

  template <class ForwardableValue> void push_back(ForwardableValue &&value);

  template <class ForwardableValue> void push_front(ForwardableValue &&value);

  std::optional<Value> try_pop_front();

  Value pop_front();

  operator std::vector<Value>() const;

#ifndef NDEBUG
  static std::atomic<size_t> s_live_nodes;
#endif
};

template <class Value> size_t queue_tm<Value>::size() const {
  return trade::atomically(trade::assume_readonly, [&]() {
    size_t n = 0;
    for (auto node = m_first.load(); node; node = node->m_next)
      n += 1;
    return n;
  });
}

template <class Value> bool queue_tm<Value>::empty() const {
  return trade::atomically(trade::assume_readonly,
                           [&]() { return !m_last.load(); });
}

template <class Value> void queue_tm<Value>::clear() {
  trade::atomically([&]() { m_first = m_last = nullptr; });
}

template <class Value>
template <class ForwardableValue>
void queue_tm<Value>::push_back(ForwardableValue &&value) {
  ptr_t<node_t> node(new node_t(std::forward<ForwardableValue>(value)));
  trade::atomically([&]() {
    if (auto prev = m_last.load())
      prev->m_next = m_last = node;
    else
      m_first = m_last = node;
  });
}

template <class Value>
template <class ForwardableValue>
void queue_tm<Value>::push_front(ForwardableValue &&value) {
  ptr_t<node_t> node(new node_t(std::forward<ForwardableValue>(value)));
  trade::atomically([&]() {
    if (auto next = m_first.load()) {
      m_first = node;
      node->m_next = next;
    } else {
      m_last = m_first = node;
    }
  });
}

template <class Value> std::optional<Value> queue_tm<Value>::try_pop_front() {
  return trade::atomically([&]() -> std::optional<Value> {
    if (auto first = m_first.load()) {
      if (auto next = first->m_next.load())
        m_first = next;
      else
        m_last = m_first = nullptr;
      return first->m_value;
    }
    return std::nullopt;
  });
}

template <class Value> Value queue_tm<Value>::pop_front() {
  return trade::atomically([&]() {
    if (auto opt_value = try_pop_front())
      return opt_value.value();
    trade::retry();
  });
}

template <class Value> queue_tm<Value>::operator std::vector<Value>() const {
  std::vector<Value> values;
  trade::atomically(trade::assume_readonly, [&]() {
    values.clear();
    for (auto node = m_first.load(); node; node = node->m_next)
      values.push_back(node->m_value);
  });
  return values;
}

#ifndef NDEBUG
template <class Value> std::atomic<size_t> queue_tm<Value>::s_live_nodes = 0;
#endif

} // namespace testing

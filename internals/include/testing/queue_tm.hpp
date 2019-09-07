#pragma once

#include "trade_v1/trade.hpp"

#include "polyfill_v1/memory.hpp"
#include <optional>
#include <vector>

#include "testing/config.hpp"

namespace testing {

/// A transactional queue for testing purposes.
template <class Value> struct queue_tm {
  using value_t = Value;

  queue_tm() = default;

  queue_tm(const queue_tm &) = delete;
  queue_tm &operator=(const queue_tm &) = delete;

  size_t size() {
    return trade::atomically(trade::assume_readonly, [&]() {
      size_t n = 0;
      for (auto node = m_first.load(); node; node = node->m_next)
        n += 1;
      return n;
    });
  }

  operator std::vector<value_t>() const {
    std::vector<value_t> values;
    trade::atomically(trade::assume_readonly, [&]() {
      values.clear();
      for (auto node = m_first.load(); node; node = node->m_next)
        values.push_back(node->m_value);
    });
    return values;
  }

  void clear() {
    trade::atomically([&]() { m_first = m_last = nullptr; });
  }

  void push_back(const value_t &value) {
    std::shared_ptr<node_t> node(new node_t{value});
    trade::atomically([&]() {
      if (auto prev = m_last.load())
        prev->m_next = m_last = node;
      else
        m_first = m_last = node;
    });
  }

  void push_front(const value_t &value) {
    std::shared_ptr<node_t> node(new node_t{value});
    trade::atomically([&]() {
      if (auto next = m_first.load()) {
        m_first = node;
        node->m_next = next;
      } else {
        m_last = m_first = node;
      }
    });
  }

  std::optional<value_t> try_pop_front() {
    return trade::atomically([&]() -> std::optional<value_t> {
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

  bool empty() {
    return trade::atomically(trade::assume_readonly,
                             [&]() { return !m_first.load(); });
  }

  value_t pop_front() {
    return trade::atomically([&]() {
      if (auto opt_value = try_pop_front())
        return opt_value.value();
      trade::retry();
    });
  }

  static std::atomic<size_t> s_live_nodes; // Only for testing purposes

private:
  struct node_t {
    trade::atom<std::shared_ptr<node_t>> m_next;
    value_t m_value;
    ~node_t() { --s_live_nodes; }
    node_t(const value_t &value) : m_value(value) { ++s_live_nodes; }
  };

  trade::atom<std::shared_ptr<node_t>> m_first;
  trade::atom<std::shared_ptr<node_t>> m_last;
};

template <class Value> std::atomic<size_t> queue_tm<Value>::s_live_nodes = 0;

} // namespace testing

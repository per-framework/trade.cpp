#pragma once

#include "trade_v1/private/access.hpp"

#include "trade_v1/synopsis.hpp"

#include <memory>

struct trade_v1::Private::transaction_base_t {
  ~transaction_base_t();
  transaction_base_t();
  clock_t m_start;
  access_base_t *m_accesses;
  uint8_t *m_alloc;
  uint8_t *m_limit;
};

struct trade_v1::Private::transaction_heap_t : transaction_base_t {
  transaction_heap_t(heap initial_size);
  void start();
  std::unique_ptr<uint8_t[]> m_block;
};

template <size_t Bytes>
struct trade_v1::Private::transaction_stack_t<trade_v1::stack_t<Bytes>>
    : transaction_base_t {
  transaction_stack_t(trade_v1::stack_t<Bytes>);
  void start();
  alignas(alignof(access_base_t)) uint8_t m_space[Bytes];
};

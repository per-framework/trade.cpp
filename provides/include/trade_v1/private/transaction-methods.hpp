#pragma once

#include "trade_v1/private/transaction.hpp"

inline trade_v1::Private::transaction_base_t::~transaction_base_t() {
  s_transaction = nullptr;
}

inline trade_v1::Private::transaction_base_t::transaction_base_t() {
  s_transaction = this;
}

inline trade_v1::Private::transaction_heap_t::transaction_heap_t(
    heap initial_size)
    : m_block(initial_size ? new uint8_t[initial_size] : nullptr) {
  m_alloc = m_limit = m_block.get() + initial_size;
}

inline void trade_v1::Private::transaction_heap_t::start() {
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

template <size_t Bytes>
trade_v1::Private::transaction_stack_t<
    trade_v1::stack_t<Bytes>>::transaction_stack_t(trade_v1::stack_t<Bytes>) {
  m_alloc = m_limit = m_space + sizeof(m_space);
}

template <size_t Bytes>
void trade_v1::Private::transaction_stack_t<trade_v1::stack_t<Bytes>>::start() {
  if (m_limit < m_alloc)
    throw std::bad_alloc();
  m_accesses = nullptr;
  m_alloc = m_space;
  m_start = s_clock;
}

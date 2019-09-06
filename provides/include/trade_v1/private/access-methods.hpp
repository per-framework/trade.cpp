#pragma once

#include "trade_v1/private/access.hpp"

template <class Value>
void trade_v1::Private::access_t<Value, false>::retain_copy() {
  new (&m_original) Value(m_current);
}

template <class Value>
void trade_v1::Private::access_t<Value, false>::retain_move() {
  new (&m_original) Value(std::move(m_current));
}

template <class Value>
void trade_v1::Private::access_t<Value, false>::destroy() {
  if (INITIAL != m_state) {
    m_current.~Value();
    if (READ + WRITTEN == m_state)
      m_original.~Value();
  }
}

template <class Value>
void trade_v1::Private::access_t<Value, true>::retain_copy() {}

template <class Value>
void trade_v1::Private::access_t<Value, true>::retain_move() {}

template <class Value>
void trade_v1::Private::access_t<Value, true>::destroy() {}

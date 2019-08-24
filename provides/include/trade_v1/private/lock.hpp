#pragma once

#include "trade_v1/private/private.hpp"

struct trade_v1::Private::lock_t {
  std::atomic<clock_t> m_clock;
  waiter_t *m_first;
};

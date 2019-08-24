#pragma once

#include "trade_v1/private/private.hpp"

struct trade_v1::Private::backoff_t {
  backoff_t();
  void operator()();
  static thread_local uint32_t s_seed;
  uint8_t m_count;
};

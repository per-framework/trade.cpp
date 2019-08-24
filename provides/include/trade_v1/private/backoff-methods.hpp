#pragma once

#include "trade_v1/private/backoff.hpp"

#include "trade_v1/config.hpp"

#include "intrinsics_v1/pause.hpp"

#include "dumpster_v1/ranqd1.hpp"

inline trade_v1::Private::backoff_t::backoff_t() : m_count(0) {}

inline void trade_v1::Private::backoff_t::operator()() {
#if defined(__clang__)
#pragma unroll 1
#endif
  for (auto n = 1 + ((s_seed = dumpster::ranqd1(s_seed)) &
                     (m_count = 2 * m_count + 1));
       n;
       --n)
    intrinsics::pause();
}

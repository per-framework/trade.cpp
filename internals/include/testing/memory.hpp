#pragma once

#include "trade_v1/trade.hpp"

namespace testing {

template <class Object> class unique {};

template <class Object> class shared {};

} // namespace testing

namespace trade_v1 {

using namespace testing;

template <class Object> class atom<unique<Object>> {

  shared<Object> load() const;
};

} // namespace trade_v1

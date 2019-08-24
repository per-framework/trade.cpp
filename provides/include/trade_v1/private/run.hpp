#pragma once

#include "trade_v1/private/private.hpp"

template <class Transaction, class Result> struct trade_v1::Private::run_t {
  template <class Config, class Action>
  static Result run(Config config, Action &&action);
};

template <class Transaction>
struct trade_v1::Private::run_t<Transaction, void> {
  template <class Config, class Action>
  static void run(Config config, Action &&action);
};

#pragma once

#include "trade_v1/private/run.hpp"
#include "trade_v1/private/transaction.hpp"

#include "trade_v1/config.hpp"

#include "dumpster_v1/finally.hpp"

template <class Transaction, class Result>
template <class Config, class Action>
Result trade_v1::Private::run_t<Transaction, Result>::run(Config config,
                                                          Action &&action) {
  Transaction transaction(config);
  while (true) {
    try {
      transaction.start();
      auto destroy_accesses =
          dumpster::finally([&]() { destroy(&transaction); });
      Result result = action();
      if (try_commit(&transaction))
        return result;
    } catch (transaction_base_t *) {
    }
  }
}

template <class Transaction>
template <class Config, class Action>
void trade_v1::Private::run_t<Transaction, void>::run(Config config,
                                                      Action &&action) {
  Transaction transaction(config);
  while (true) {
    try {
      transaction.start();
      auto destroy_accesses =
          dumpster::finally([&]() { destroy(&transaction); });
      action();
      if (try_commit(&transaction))
        return;
    } catch (transaction_base_t *) {
    }
  }
}

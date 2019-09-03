#include "trade_v1/trade.hpp"

#include "testing_v1/test.hpp"

#include <thread>

using namespace trade_v1;
using namespace testing_v1;

constexpr size_t n_to_eat = 100000;
constexpr size_t n_philosophers = 5;

struct fork {
  fork() : on_table(true) {}

  alignas(64) atom<bool> on_table;
};

auto dining_test = test([]() {
  atom<size_t> done = 0;

  fork forks[n_philosophers];

  for (size_t philosopher = 0; philosopher < n_philosophers; ++philosopher)
    std::thread([&, philosopher]() {
      auto &left = forks[philosopher];
      auto &right = forks[(philosopher + 1) % n_philosophers];

      for (size_t e = 0; e < n_to_eat; ++e) {
        atomically([&]() {
          bool &left_on_table = left.on_table.ref();
          bool &right_on_table = right.on_table.ref();
          if (left_on_table && right_on_table)
            left_on_table = right_on_table = false;
          else
            retry();
        });

        atomically([&]() { left.on_table = right.on_table = true; });
      }

      atomically([&]() { ++done.ref(); });
    }).detach();

  atomically([&]() {
    if (done != n_philosophers)
      retry();
  });
});

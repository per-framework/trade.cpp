#include "trade_v1/trade.hpp"

#include "testing_v1/test.hpp"

#include <algorithm>
#include <thread>

using namespace testing_v1;
using namespace trade_v1;

auto contention_test = test([]() {
  const size_t n_threads = std::thread::hardware_concurrency();
  const size_t n_ops = 100000;

  atom<size_t> n_threads_started = 0, n_threads_stopped = 0;

  constexpr size_t n_atoms = 7;

  atom<int> atoms[n_atoms];
  for (size_t i = 0; i < n_atoms; ++i)
    atomically([&]() { atoms[i] = 0; });

  for (size_t t = 0; t < n_threads; ++t) {
    std::thread([&]() {
      atomically([&]() { n_threads_started.ref() += 1; });
      atomically([&]() {
        if (n_threads_started != n_threads)
          retry();
      });

      for (size_t o = 0; o < n_ops; ++o) {
        auto i = std::rand() % n_atoms;
        auto j = i;
        while (i == j)
          j = std::rand() % n_atoms;

        atomically(stack<128>, [&]() {
          int &x = atoms[i].ref();
          int &y = atoms[j].ref();
          std::swap(--x, ++y);
        });
      }

      atomically([&]() { n_threads_stopped.ref() += 1; });
    }).detach();
  }

  atomically(assume_readonly, [&]() {
    if (n_threads_stopped != n_threads)
      retry();
  });

  auto [sum, non_zeroes] = atomically(assume_readonly, [&]() {
    int sum = 0;
    int non_zeroes = 0;

    for (size_t i = 0; i < n_atoms; ++i) {
      int value = atoms[i];
      non_zeroes += 0 != value;
      sum += value;
    }

    return std::make_tuple(sum, non_zeroes);
  });

  verify(non_zeroes);
  verify(sum == 0);
});

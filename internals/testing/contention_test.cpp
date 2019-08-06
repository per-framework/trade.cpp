#include "trade_v1/trade.hpp"

#include "testing_v1/test.hpp"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>

using namespace testing_v1;
using namespace trade_v1;

auto contention_test = test([]() {
  const size_t n_threads = std::thread::hardware_concurrency();
  const size_t n_ops = 100000;

  std::mutex mutex;
  std::condition_variable condition;

  size_t n_threads_started = 0;
  size_t n_threads_stopped = 0;

  constexpr size_t n_atoms = 7;

  atom<int> atoms[n_atoms];
  atomically([&]() {
    for (size_t i = 0; i < n_atoms; ++i)
      atoms[i] = 0;
  });

  for (size_t t = 0; t < n_threads; ++t) {
    std::thread([&]() {
      {
        std::unique_lock<std::mutex> guard(mutex);
        n_threads_started += 1;
        condition.notify_all();
        while (n_threads_started != n_threads)
          condition.wait(guard);
      }

      for (size_t o = 0; o < n_ops; ++o) {
        auto i = std::rand() % n_atoms;
        auto j = i;
        while (i == j)
          j = std::rand() % n_atoms;

        atomically([&]() {
          int x = atoms[i];
          int y = atoms[j];
          atoms[j] = x - 1;
          atoms[i] = y + 1;
        });
      }

      {
        std::unique_lock<std::mutex> guard(mutex);
        n_threads_stopped += 1;
        condition.notify_all();
      }
    }).detach();
  }

  {
    std::unique_lock<std::mutex> guard(mutex);
    while (n_threads_stopped != n_threads)
      condition.wait(guard);
  }

  auto [sum, non_zeroes] = atomically([&]() {
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

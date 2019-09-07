#include "testing/queue_tm.hpp"

#include "testing_v1/test.hpp"

#include <algorithm>
#include <thread>

using namespace testing_v1;
using namespace trade_v1;
using namespace testing;

auto retry_test = test([]() {
  queue_tm<size_t> queues[2];

  queues[0].push_back(0);

  const size_t n_rounds = 1000;
  const size_t n_threads =
      std::max(std::thread::hardware_concurrency() - 1, 2u);

  std::vector<std::thread> threads;

  for (size_t t = 0; t < n_threads; ++t)
    threads.push_back(std::thread([&]() {
      for (size_t i = 0; i < n_rounds; ++i)
        queues[0].push_back(queues[1].pop_front() + 1);
    }));

  for (size_t i = 0; i < n_rounds * n_threads; ++i)
    queues[1].push_back(queues[0].pop_front() + 1);

  verify(2 * n_threads * n_rounds == queues[0].pop_front());

  for (auto &thread : threads)
    thread.join();
});

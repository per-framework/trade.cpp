#include "testing/queue.hpp"

#include "testing_v1/test.hpp"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

using namespace testing_v1;
using namespace trade_v1;
using namespace testing;

auto dynamic_test = test([]() {
  const size_t n_threads =
      std::max(std::thread::hardware_concurrency() - 1, 2u);
  const size_t n_ops = 10000;

  std::mutex mutex;
  std::condition_variable condition;

  size_t n_threads_started = 0, n_threads_stopped = 0;

  queue_t<int> queues[2];

  constexpr size_t n_values = 10;
  for (size_t i = 0; i < n_values; ++i)
    atomically([&]() { queues[0].push_back(i); });

  for (size_t t = 0; t < n_threads; ++t) {
    std::thread([&]() {
      {
        std::unique_lock<std::mutex> guard(mutex);
        n_threads_started += 1;
        condition.notify_all();
        while (n_threads_started != n_threads)
          condition.wait(guard);
      }

      size_t n = n_ops;
      while (n) {
        int r = std::rand();

        queue_t<int> &q1 = queues[r & 1];
        queue_t<int> &q2 = queues[(r & 2) >> 1];

        // Here we perform a composed transaction that tries to move a value
        // from one queue to (possibly) another queue atomically:
        if (atomically([&]() {
              if (auto opt_v = q1.try_pop_front()) {
                auto v = opt_v.value();
                if (r & 4)
                  q2.push_back(v);
                else
                  q2.push_front(v);
                return true;
              } else {
                return false;
              }
            }))
          n -= 1;
      }

      {
        std::unique_lock<std::mutex> guard(mutex);
        n_threads_stopped += 1;
        condition.notify_all();
      }
    }).detach();
  }

  {
    size_t i = 0;
    while (n_threads_stopped != n_threads) {
      atomically(assume_readonly, [&]() {
        // We must not be able observe an invalid queue size inside a
        // transaction:
        size_t n0 = queues[0].size();
        size_t n1 = queues[1].size();
        verify(n0 + n1 == n_values);
        if (i & 8)
          queues[!(n0 < n1)].push_back(queues[n0 < n1].pop_front());
      });
      i += 1;
    }
  }

  while (auto v = queues[1].try_pop_front())
    queues[0].push_back(v.value());
  std::vector<int> values = queues[0];
  queues[0].clear();

  std::sort(values.begin(), values.end());
  for (size_t i = 0; i < n_values; ++i)
    verify(values[i] == static_cast<int>(i));

  {
    queue_t<int> q;
    q.push_back(1);

    {
      int result = 0;
      try {
        atomically([&]() {
          q.push_back(2);
          throw 101;
        });
      } catch (int v) {
        result = v;
      }
      verify(result == 101);
    }

    try {
      verify(atomically([&]() -> bool {
        if (q.try_pop_front())
          throw 42;
        return false;
      }));
    } catch (int v) {
      verify(v == 42);
    }

    verify(1 == q.size());
    // Intentionally incorrectly assumes readonly:
    atomically(assume_readonly, [&]() {
      if (!q.is_empty())
        q.pop_front();
    });
    verify(0 == q.size());
  }

  verify(!queue_t<int>::s_live_nodes);
});

#include "testing/hash_map_tm.hpp"

#include "testing_v1/test.hpp"

#include "dumpster_v1/ranqd1.hpp"

#include "polyfill_v1/memory.hpp"
#include <thread>

using namespace testing_v1;

using namespace testing;
using namespace trade;

auto hash_map_test = test([]() {
  const size_t n_threads = std::thread::hardware_concurrency();
  const size_t n_ops = 100000;
  const uint32_t max_keys = 31;

  using hash_map_tm_type = hash_map_tm<uint32_t, size_t>;

  hash_map_tm_type map;

  atom<size_t> done(0);

  auto start = std::chrono::high_resolution_clock::now();

  for (size_t t = 0; t < n_threads; ++t)
    std::thread([&, t]() {
      auto s = static_cast<uint32_t>(t);

      for (size_t i = 0; i < n_ops; ++i) {
        uint32_t key = (s = dumpster::ranqd1(s)) % max_keys;
        map.add_or_set(key, t, trade::stack<8192>);
      }

      atomically([&]() { done.ref() += 1; });
    }).detach();

  atomically(assume_readonly, [&]() {
    if (done != n_threads)
      retry();
  });

  std::chrono::duration<double> elapsed =
      std::chrono::high_resolution_clock::now() - start;
  auto n_total = n_ops * n_threads;
  fprintf(stderr,
          "%f Mops in %f s = %f Mops/s\n",
          n_total / 1000000.0,
          elapsed.count(),
          n_total / elapsed.count() / 1000000.0);

  verify(map.size() == max_keys);

  {
    hash_map_tm_type other;
    map.swap(other);
    verify(other.size() == max_keys);
    verify(map.size() == 0);
    other.clear();
  }

#ifndef NDEBUG
  verify(!hash_map_tm_type::s_live_nodes);
#endif
});

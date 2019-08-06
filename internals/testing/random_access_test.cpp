#include "trade_v1/trade.hpp"

#include "testing_v1/test.hpp"

#include <cstdlib>

using namespace testing_v1;
using namespace trade_v1;

auto random_access_test = test([]() {
  atom<int> xAs[11] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 8};

  auto randomIx = []() { return rand() % 11; };

  for (auto i = 0; i < 100; ++i)
    atomically(heap(100), [&]() {
      xAs[randomIx()].load();
      xAs[randomIx()].load();
      xAs[randomIx()].load();
      xAs[randomIx()].load();
      xAs[randomIx()].load();
      xAs[randomIx()].load();
      xAs[randomIx()] = xAs[randomIx()].load();
    });
});

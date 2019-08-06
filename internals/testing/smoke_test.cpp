#include "trade_v1/trade.hpp"

#include <memory>

#include "testing_v1/test.hpp"

using namespace testing_v1;
using namespace trade_v1;

auto smoke_test = test([]() {
  atom<int> xA = 1;
  atom<float> yA = 2.f;
  atom<int> zA = 3;

  verify(atomically(assume_readonly, []() { return true; }));

  {
    verify(1 == xA.unsafe_load());
    verify(2 == yA.unsafe_load());
    verify(3 == zA.unsafe_load());
  }

  {
    atomically([&]() {
      int x = xA;
      int z = zA;
      xA = z;
      zA = x;
    });

    verify(3 == xA.unsafe_load());
    verify(2 == yA.unsafe_load());
    verify(1 == zA.unsafe_load());
  }

  {
    auto r = atomically([&]() { return xA + 1; });

    verify(r == 4);
    verify(3 == xA.unsafe_load());
    verify(2 == yA.unsafe_load());
    verify(1 == zA.unsafe_load());
  }

  {
    auto r = atomically([&]() { return xA + yA; });

    verify(r == 5.f);
    verify(3 == xA.unsafe_load());
    verify(2 == yA.unsafe_load());
    verify(1 == zA.unsafe_load());
  }
});

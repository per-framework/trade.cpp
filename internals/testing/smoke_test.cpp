#include "trade_v1/trade.hpp"

#include "polyfill_v1/memory.hpp"

#include "testing_v1/test.hpp"

using namespace testing_v1;
using namespace trade_v1;

auto smoke_test = test([]() {
  atom<int> xA = 1;
  atom<float> yA = 2.f;
  atom<int> zA = 3;
  atom<std::shared_ptr<int>> p(std::make_shared<int>(32));

  verify(atomically(assume_readonly, []() { return true; }));

  {
    verify(1 == xA.unsafe_load());
    verify(2 == yA.unsafe_load());
    verify(3 == zA.unsafe_load());
  }

  {
    atomically([&]() {
      int x = xA;
      int z = zA.load();
      xA = z;
      zA.store(x);
    });

    verify(3 == xA.unsafe_load());
    verify(2 == yA.unsafe_load());
    verify(1 == zA.unsafe_load());
  }

  {
    auto r = atomically([&]() { return xA.ref() + 1; });

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

  { verify(!!p.unsafe_load()); }

  {
    struct TriviallyCopyable {
      int x;
      double y;
      size_t z;
    };

    static_assert(!std::atomic<TriviallyCopyable>::is_always_lock_free);

    atom<TriviallyCopyable> tc({3, 0.14, 592});

    verify(tc.unsafe_load().x == 3);
    verify(tc.unsafe_load().y == 0.14);
    verify(tc.unsafe_load().z == 592);
  }

  {
    verify(*p.unsafe_load() + xA.unsafe_load() ==
           atomically(heap(4), [&]() { return *p.load() + xA; }));
  }

  {
    try {
      verify(p.unsafe_load() !=
             atomically(stack<1>, [&]() { return p.load(); }));
    } catch (const std::bad_alloc &) {
    }
  }
});

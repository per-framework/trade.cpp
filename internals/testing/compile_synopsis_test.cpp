#include "trade_v2/synopsis.hpp"

static_assert(std::is_nothrow_swappable_v<trade_v2::sole_ptr<int>>);
static_assert(sizeof(trade_v2::atom<int>));
static_assert(sizeof(trade_v2::atom<trade_v2::sole_ptr<int>>));

#include "trade_v2/library.hpp"

#include <memory>

int main() {
  {
    using t = int;
    trade_v2::atom<t> x;
    static_assert(trade_v2::atom_traits<t>::is_atomic);
  }

  {
    using t = std::pair<int, bool>;
    trade_v2::atom<t> x(101, false);
  }

  {
    struct t {
      size_t more;
      size_t than;
      size_t two_words;
    };

    static_assert(trade_v2::atom_traits<t>::is_trivial);
  }

  {
    using t = std::shared_ptr<int>;
    trade_v2::atom<t> x;
    static_assert(!trade_v2::atom_traits<t>::is_trivial &&
                  !trade_v2::atom_traits<t>::is_atomic);
  }

  {
    using t = trade_v2::sole_ptr<int>;
    trade_v2::atom<t> x;
    static_assert(trade_v2::atom_traits<t>::is_atomic);
  }

  return 0;
}

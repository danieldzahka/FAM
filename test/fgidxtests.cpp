#include <catch2/catch.hpp>
#include <constants.hpp>
#include <string>

#include <fgidx.hpp>

namespace {
std::string const fgidx_testfile = FGIDX_TEST1;
}

TEST_CASE("Test .idx reading", "[fgidx]")
{
  uint32_t const v_max = 5;
  uint64_t const edges = 6;
  auto const idx = fgidx::dense_idx::CreateInstance(fgidx_testfile, edges);

  REQUIRE(v_max == idx.v_max);

  REQUIRE(idx[0].begin == 0);
  REQUIRE(idx[0].end_exclusive == 3);
  REQUIRE(idx[1].begin == 3);
  REQUIRE(idx[1].end_exclusive == 5);
  REQUIRE(idx[2].begin == 5);
  REQUIRE(idx[2].end_exclusive == 5);
  REQUIRE(idx[3].begin == 5);
  REQUIRE(idx[3].end_exclusive == 6);
  REQUIRE(idx[4].begin == 6);
  REQUIRE(idx[4].end_exclusive == 6);
  REQUIRE(idx[5].begin == 6);
  REQUIRE(idx[5].end_exclusive == 6);
}

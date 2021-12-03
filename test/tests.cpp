#include <catch2/catch.hpp>
#include <FAM.hpp>
#include <constants.hpp>
#include <string>

namespace {
const std::string host = MEMADDR;
const std::string port = PORT;
}// namespace

TEST_CASE("RPC Consruction", "[RPC]")
{
  using namespace FAM::client;
  REQUIRE_NOTHROW(RPC_client{ host, port });
}

TEST_CASE("RPC PING", "[RPC]")
{
  using namespace FAM;
  using namespace client;

  RPC_client client{ host, port };

  SECTION("Message: ping")
  {
    REQUIRE_NOTHROW(client.ping());
  }

  // SECTION("Message: allocate_region")
  // {
  //   auto const response = client.allocate_region();
  //   REQUIRE(response.status == response::status::OK);
  // }

  // SECTION("Message: mmap_file")
  // {
  //   auto const response = client.mmap_file();
  //   REQUIRE(response.status == response::status::OK);
  // }

  // SECTION("Message: create_QP")
  // {
  //   auto const response = client.create_QP();
  //   REQUIRE(response.status == response::status::OK);
  // }
}

TEST_CASE("RPC Allocate Region", "[RPC]")
{
  using namespace FAM;
  using namespace client;

  RPC_client client{ host, port };

  // SECTION("Message: ping")
  // {
  //   REQUIRE_NOTHROW(client.ping());
  // }

  SECTION("Message: allocate_region")
  {
    std::uint64_t const length = 117;
    auto const response = client.allocate_region(request::allocate_region{length});
    REQUIRE(response.length == length);
  }
}

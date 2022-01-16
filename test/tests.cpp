#include <catch2/catch.hpp>
#include <FAM.hpp>
#include <constants.hpp>
#include <string>

namespace {
const std::string host = MEMADDR;
const std::string port = PORT;
const std::string rdma_host = "192.168.12.2";
const std::string rdma_port = "35287";
}// namespace

TEST_CASE("RPC Consruction", "[RPC]")
{
  using namespace FAM::client;
  REQUIRE_NOTHROW(FAM_control{ "0.0.0.0:50051", rdma_host, rdma_port });
}

TEST_CASE("RPC PING", "[RPC]")
{
  using namespace FAM::client;

  FAM_control client{ "0.0.0.0:50051", rdma_host, rdma_port };

  SECTION("Message: ping") { REQUIRE_NOTHROW(client.ping()); }
}

TEST_CASE("RPC Allocate Region", "[RPC]")
{
  using namespace FAM::client;

  FAM_control client{ "0.0.0.0:50051", rdma_host, rdma_port };
  REQUIRE_NOTHROW(client.create_connection());
  REQUIRE_NOTHROW(client.create_connection());

  SECTION("Message: allocate_region")
  {
    std::uint64_t const length = 117;
    // auto const response =
    REQUIRE_NOTHROW(client.allocate_region(length));
    REQUIRE_NOTHROW(client.allocate_region(length));
    // REQUIRE_NOTHROW(client.allocate_region(length));
    // REQUIRE_NOTHROW(client.allocate_region(length));
    // REQUIRE(response.length == length);
  }
}

// TEST_CASE("RDMA client consruction", "[RDMA]")
// {
//   REQUIRE_NOTHROW(FAM::RDMA::client{ rdma_host, rdma_port });
// }

// TEST_CASE("RDMA client connection", "[RDMA]")
// {
//   FAM::RDMA::client client{ rdma_host, rdma_port };
//   REQUIRE_NOTHROW(client.create_connection());
// }

// TEST_CASE("Client Create RDMA Buffer", "[RDMA]")
// {
//   FAM::RDMA::client client{ rdma_host, rdma_port };
//   REQUIRE_NOTHROW(client.create_connection());
//   REQUIRE_NOTHROW(client.create_region(771, false, false));
// }

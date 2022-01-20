#include <catch2/catch.hpp>
#include <FAM.hpp>
#include <constants.hpp>
#include <string>

#include <chrono>
#include <thread>

namespace {
const std::string memserver_addr = MEMADDR;
const std::string rdma_host = "192.168.12.2";
const std::string rdma_port = "35287";
const std::string mmap_test1 = MMAP_TEST1;
}// namespace

TEST_CASE("RPC Consruction", "[RPC]")
{
  using namespace FAM::client;
  REQUIRE_NOTHROW(FAM_control{ memserver_addr, rdma_host, rdma_port, 1 });
}

TEST_CASE("RPC PING", "[RPC]")
{
  using namespace FAM::client;
  FAM_control client{ memserver_addr, rdma_host, rdma_port, 1 };

  SECTION("Message: ping") { REQUIRE_NOTHROW(client.ping()); }
}

TEST_CASE("RPC Allocate Region", "[RPC]")
{
  using namespace FAM::client;
  FAM_control client{ memserver_addr, rdma_host, rdma_port, 1 };

  SECTION("Message: allocate_region")
  {
    std::uint64_t const length = 117;
    auto const response = client.allocate_region(length);
    REQUIRE(response.length == length);
  }
}

TEST_CASE("Client Create RDMA Buffer", "[RDMA]")
{
  using namespace FAM::client;
  FAM_control client{ memserver_addr, rdma_host, rdma_port, 1 };

  REQUIRE_NOTHROW(client.create_region(771, false, false));
}

TEST_CASE("RDMA Write", "[RDMA]")
{
  using namespace FAM::client;
  FAM_control client{ memserver_addr, rdma_host, rdma_port, 1 };

  auto const [laddr, l1, lkey] = client.create_region(1024, false, false);
  auto const [raddr, l2, rkey] = client.allocate_region(1024);

  volatile int *p = reinterpret_cast<int volatile *>(laddr);
  constexpr auto magic = 0x0FFFFFFF;
  *p = magic;

  client.read(const_cast<int *>(p), raddr, 4, lkey, rkey, 0);
  while (*p == magic) {}

  REQUIRE(*p == 0);
}

TEST_CASE("RDMA mmap", "[RDMA]")
{
  using namespace FAM::client;
  FAM_control client{ memserver_addr, rdma_host, rdma_port, 1 };

  uint64_t constexpr filesize = 40;// bytes

  auto const [laddr, l1, lkey] = client.create_region(filesize, false, false);
  auto const [raddr, l2, rkey] = client.mmap_remote_file(mmap_test1);

  REQUIRE(l2 == filesize);

  volatile int *p = reinterpret_cast<int volatile *>(laddr);
  constexpr auto magic = 0x0FFFFFFF;
  *p = magic;

  client.read(const_cast<int *>(p), raddr, filesize, lkey, rkey, 0);
  while (*p == magic) {}

  for (int i = 0; i < 10; ++i) REQUIRE(p[i] == i);
}

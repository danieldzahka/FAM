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
  REQUIRE_NOTHROW(FAM::FamControl{ memserver_addr, rdma_host, rdma_port, 1 });
}

TEST_CASE("RPC PING", "[RPC]")
{
  FAM::FamControl client{ memserver_addr, rdma_host, rdma_port, 1 };

  SECTION("Message: Ping") { REQUIRE_NOTHROW(client.Ping()); }
}

TEST_CASE("RPC Allocate Region", "[RPC]")
{
  FAM::FamControl client{ memserver_addr, rdma_host, rdma_port, 1 };

  SECTION("Message: AllocateRegion")
  {
    std::uint64_t const length = 117;
    auto const response = client.AllocateRegion(length);
    REQUIRE(response.length == length);
  }
}

TEST_CASE("Client Create rdma Buffer", "[rdma]")
{
  FAM::FamControl client{ memserver_addr, rdma_host, rdma_port, 1 };

  REQUIRE_NOTHROW(client.CreateRegion(771, false, false));
}

TEST_CASE("rdma Write", "[rdma]")
{
  FAM::FamControl client{ memserver_addr, rdma_host, rdma_port, 1 };

  auto const [laddr, l1, lkey] = client.CreateRegion(1024, false, false);
  auto const [raddr, l2, rkey] = client.AllocateRegion(1024);

  volatile int *p = reinterpret_cast<int volatile *>(laddr);
  constexpr auto magic = 0x0FFFFFFF;
  *p = magic;

  client.Read(const_cast<int *>(p), raddr, 4, lkey, rkey, 0);
  while (*p == magic) {}

  REQUIRE(*p == 0);
}

TEST_CASE("rdma mmap", "[rdma]")
{
  FAM::FamControl client{ memserver_addr, rdma_host, rdma_port, 1 };

  uint64_t constexpr filesize = 40;// bytes

  auto const [laddr, l1, lkey] = client.CreateRegion(filesize, false, false);
  auto const [raddr, l2, rkey] = client.MmapRemoteFile(mmap_test1);

  REQUIRE(l2 == filesize);

  volatile int *p = reinterpret_cast<int volatile *>(laddr);
  constexpr auto magic = 0x0FFFFFFF;

  SECTION("Single Read")
  {
    *p = magic;
    client.Read(const_cast<int *>(p), raddr, filesize, lkey, rkey, 0);
    while (*p == magic) {}
  }
  SECTION("Vector Read")
  {
    p[0] = magic;
    p[5] = magic;
    // 0-4 5-9
    auto const length = 5 * sizeof(uint32_t);
    std::vector<FAM::FamSegment> v = { FAM::FamSegment{ raddr, length },
      FAM::FamSegment{ raddr + length, length } };
    client.Read(const_cast<int *>(p), raddr, filesize, lkey, rkey, 0);
    while (p[0] == magic || p[5] == magic) {}
  }

  for (int i = 0; i < 10; ++i) REQUIRE(p[i] == i);
}
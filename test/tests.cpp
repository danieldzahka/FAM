#include <catch2/catch.hpp>
#include <FAM.hpp>

#include <string>

namespace {
const std::string host = "0.0.0.0";
const std::string port = "8080";
}// namespace

TEST_CASE("Testing RPC Consruction", "[RPC]")
{
  using namespace FAM::client;
  REQUIRE_NOTHROW(RPC_client{ host, port });
}

TEST_CASE("Testing RPC Layer", "[RPC]")
{
  using namespace FAM;
  using namespace client;

  RPC_client client{ host, port };

  SECTION("Message: ping")
  {
    auto const response = client.ping();
    REQUIRE(response.status == response::status::OK);
  }

  SECTION("Message: allocate_region")
  {
    auto const response = client.allocate_region();
    REQUIRE(response.status == response::status::OK);
  }

  SECTION("Message: mmap_file")
  {
    auto const response = client.mmap_file();
    REQUIRE(response.status == response::status::OK);
  }

  SECTION("Message: create_QP")
  {
    auto const response = client.create_QP();
    REQUIRE(response.status == response::status::OK);
  }
}

#include <FAM.hpp>
#include <stdexcept>

using namespace FAM::client;

RPC_client::RPC_client(std::string, std::string)
{
  throw std::runtime_error("Not yet implemented");
}
response::ping RPC_client::ping()
{
  throw std::runtime_error("Not yet implemented");
}
response::allocate_region RPC_client::allocate_region()
{
  throw std::runtime_error("Not yet implemented");
}
response::mmap_file RPC_client::mmap_file()
{
  throw std::runtime_error("Not yet implemented");
}
response::create_QP RPC_client::create_QP()
{
  throw std::runtime_error("Not yet implemented");
}

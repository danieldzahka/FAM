#ifndef _FAM_H_
#define _FAM_H_

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <string>
#include <memory>

namespace FAM {
namespace beast = boost::beast;// from <boost/beast.hpp>
namespace http = beast::http;// from <boost/beast/http.hpp>
namespace websocket = beast::websocket;// from <boost/beast/websocket.hpp>
namespace net = boost::asio;// from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;// from <boost/asio/ip/tcp.hpp>

struct QP
{
};// qp owner
namespace response {
  enum class status {
    OK,
    FAIL,
  };

  struct ping
  {
  };

  struct allocate_region
  {
    std::uint64_t const addr;
    std::uint64_t const length;
    allocate_region(std::uint64_t t_addr, std::uint64_t t_length)
      : addr{ t_addr }, length{ t_length }
    {}
  };

  struct mmap_file
  {
    std::uint64_t const addr;
    std::uint64_t const edges;
    std::uint32_t const verts;
    mmap_file(std::uint64_t t_addr,
      std::uint64_t t_edges,
      std::uint32_t t_verts)
      : addr{ t_addr }, edges{ t_edges }, verts{ t_verts }
    {}
  };

  struct create_QP
  {
  };
}// namespace response

namespace request {
  enum class type {
    PING,
    ALLOCATE_REGION,
    MMAP_FILE,
    CREATE_QP,
  };

  struct ping
  {
    using response_type = FAM::response::ping;
  };

  struct allocate_region
  {
    using response_type = FAM::response::allocate_region;
    std::uint64_t size;
  };

  struct mmap_file
  {
    using response_type = FAM::response::mmap_file;
    std::string fname;
    explicit mmap_file(std::string const &t_fname) : fname{ t_fname } {}
  };

  struct create_QP
  {
    using response_type = FAM::response::create_QP;
  };
}// namespace request

namespace client {
  class RPC_client
  {
    net::io_context ioc;
    tcp::resolver resolver{ ioc };
    websocket::stream<tcp::socket> ws{ ioc };

  public:
    RPC_client(std::string host, std::string port);
    ~RPC_client();
    response::ping ping();
    response::allocate_region allocate_region(
      request::allocate_region const req);
    response::mmap_file mmap_file();
    response::create_QP create_QP();
  };

  void create_session();// factory that tries to do all connection work, then
                        // returns an object containing all communication
                        // resources, both beast and IB. The objects destructor
                        // knows how to tare down the connections.
}// namespace client

namespace server {
  class RPC_session
  {
  };
  void run(std::string const &host,
    std::string const &port);// throw exception if something bad happens
}// namespace server

namespace RDMA {
  class client_impl;// Forward Declare
  class client
  {
    std::unique_ptr<client_impl> pimpl;

  public:
    client(std::string const &t_host, std::string const &t_port);
    ~client();
    void create_connection();
    void *create_region(std::uint64_t const t_size,
      bool const use_HP,
      bool const write_allowed);
  };
}// namespace RDMA
}// namespace FAM

#endif

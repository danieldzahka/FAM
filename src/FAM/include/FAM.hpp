#ifndef _FAM_H_
#define _FAM_H_

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <vector>
#include <string>

namespace FAM {
namespace beast = boost::beast;// from <boost/beast.hpp>
namespace http = beast::http;// from <boost/beast/http.hpp>
namespace websocket = beast::websocket;// from <boost/beast/websocket.hpp>
namespace net = boost::asio;// from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;// from <boost/asio/ip/tcp.hpp>

struct QP
{
};// qp owner


namespace client {
  namespace response {
    enum class status {
      OK,
      FAIL,
    };

    struct ping
    {
      FAM::client::response::status const status;
      ping(FAM::client::response::status t_status) : status{ t_status } {}
    };

    struct allocate_region
    {
      FAM::client::response::status const status;
      std::uint64_t const addr;
      allocate_region(FAM::client::response::status t_status,
        std::uint64_t t_addr)
        : status{ t_status }, addr{ t_addr }
      {}
    };

    struct mmap_file
    {
      FAM::client::response::status const status;
      std::uint64_t const addr;
      std::uint64_t const edges;
      std::uint32_t const verts;
      mmap_file(FAM::client::response::status t_status,
        std::uint64_t t_addr,
        std::uint64_t t_edges,
        std::uint32_t t_verts)
        : status{ t_status }, addr{ t_addr }, edges{ t_edges }, verts{ t_verts }
      {}
    };

    struct create_QP
    {
      FAM::client::response::status const status;
      create_QP(FAM::client::response::status t_status) : status{ t_status } {}
    };

  }// namespace response

  class RPC_client
  {
    net::io_context ioc;
    tcp::resolver resolver{ ioc };
    websocket::stream<tcp::socket> ws{ ioc };

  public:
    RPC_client(std::string host, std::string port);
    response::ping ping();
    response::allocate_region allocate_region();
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
}// namespace FAM

#endif

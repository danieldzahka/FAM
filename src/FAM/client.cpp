#include <FAM.hpp>
#include <stdexcept>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/json.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace FAM::client;

namespace {
boost::json::value transmit_then_recv(websocket::stream<tcp::socket> &ws,
  boost::json::value const &jv)
{
  auto const text = boost::json::serialize(jv);
  ws.write(net::buffer(std::string(text)));
  beast::flat_buffer buffer;
  ws.read(buffer);
  return boost::json::parse(beast::buffers_to_string(buffer.data()));
}
}// namespace

RPC_client::RPC_client(std::string host, std::string port)
{
  auto const text = "This is a test message";
  auto const results = resolver.resolve(host, port);
  auto ep = net::connect(ws.next_layer(), results);
  host += ':' + std::to_string(ep.port());
  ws.set_option(
    websocket::stream_base::decorator([](websocket::request_type &req) {
      req.set(http::field::user_agent,
        std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
    }));
  ws.handshake(host, "/");
}

RPC_client::~RPC_client { ws.close(websocket::close_code::normal); }

response::ping RPC_client::ping()
{
  boost::json::value const jv = {
    { "type", "ping" },
  };
  auto const resp = transmit_then_recv(this->ws, jv);

  return response::ping {}
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

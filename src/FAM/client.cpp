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

namespace beast = boost::beast;// from <boost/beast.hpp>
namespace http = beast::http;// from <boost/beast/http.hpp>
namespace websocket = beast::websocket;// from <boost/beast/websocket.hpp>
namespace net = boost::asio;// from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;// from <boost/asio/ip/tcp.hpp>

using namespace FAM::client;

namespace {
auto type_to_string(FAM::request::type const type)
{
  using namespace FAM;
  switch (type) {
  case request::type::PING:
    return "PING";
  case request::type::ALLOCATE_REGION:
    return "ALLOCATE_REGION";
  case request::type::MMAP_FILE:
    return "MMAP_FILE";
  case request::type::CREATE_QP:
    return "CREATE_QP";
  }
  throw std::runtime_error("type_to_string: Invalid Message Type");
}

auto string_to_status(std::string const &s)
{
  using namespace FAM;
  if (s == "OK") return response::status::OK;
  if (s == "FAIL") return response::status::FAIL;
  throw std::runtime_error("string_to_status: Invalid Status String");
}

}// namespace

namespace FAM {
namespace request {
  // struct -> json conversions
  void tag_invoke(boost::json::value_from_tag,
    boost::json::value &jv,
    ping const &)
  {
    jv = {};
  }

  void tag_invoke(boost::json::value_from_tag,
    boost::json::value &jv,
    allocate_region const &req)
  {
    jv = {{"size", req.size}};
  }

}// namespace request
namespace response {
  // json -> struct conversions
  auto tag_invoke(boost::json::value_to_tag<ping>, boost::json::value const &)
  {
    return ping{};
  }
  auto tag_invoke(boost::json::value_to_tag<allocate_region>,
    boost::json::value const &jv)
  {
    using namespace boost::json;
    auto const &obj = jv.as_object();
    return allocate_region{ value_to<std::uint64_t>(obj.at("addr")),
      value_to<std::uint64_t>(obj.at("length")) };
  }
}// namespace response
}// namespace FAM

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

template<typename Message>
auto make_rpc(websocket::stream<tcp::socket> &ws,
  Message const &m,
  FAM::request::type const type)
{
  using namespace boost::json;
  auto const jv = value_from(m);
  value const message = { { "type", type_to_string(type) }, { "message", jv } };
  auto const resp = transmit_then_recv(ws, message);
  auto const status =
    string_to_status(value_to<std::string>(resp.at("status")));
  if (status == FAM::response::status::FAIL)
    throw std::runtime_error("rpc: status FAIL");
  auto const resp_val = resp.at("response");
  return value_to<typename Message::response_type>(resp_val);
}

}// namespace

RPC_client::RPC_client(std::string host, std::string port)
{
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

RPC_client::~RPC_client() { this->ws.close(websocket::close_code::normal); }

FAM::response::ping RPC_client::ping()
{
  return make_rpc(this->ws, FAM::request::ping{}, FAM::request::type::PING);
}
FAM::response::allocate_region RPC_client::allocate_region(FAM::request::allocate_region const req)
{
  return make_rpc(this->ws, req, FAM::request::type::ALLOCATE_REGION);
}
FAM::response::mmap_file RPC_client::mmap_file()
{
  throw std::runtime_error("Not yet implemented");
}
FAM::response::create_QP RPC_client::create_QP()
{
  throw std::runtime_error("Not yet implemented");
}

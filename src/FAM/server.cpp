#include <FAM.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/json.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

#include <spdlog/spdlog.h>//delete maybe

namespace beast = boost::beast;// from <boost/beast.hpp>
namespace http = beast::http;// from <boost/beast/http.hpp>
namespace websocket = beast::websocket;// from <boost/beast/websocket.hpp>
namespace net = boost::asio;// from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;// from <boost/asio/ip/tcp.hpp>

namespace {
auto string_to_type(std::string const &s)
{
  using namespace FAM;
  if (s == "PING") return request::type::PING;
  if (s == "ALLOCATE_REGION") return request::type::ALLOCATE_REGION;
  if (s == "MMAP_FILE") return request::type::MMAP_FILE;
  if (s == "CREATE_QP") return request::type::CREATE_QP;
  throw std::runtime_error("string_to_type: Invalid Message String");
}

auto handle(FAM::request::ping const)
{
  using namespace FAM::response;
  return boost::json::value{ { "status", "OK" },
    { "response", boost::json::value_from(ping{}) } };
}
auto handle(FAM::request::allocate_region const req)
{
  using namespace FAM::response;
  auto const length = req.size;
  return boost::json::value{ { "status", "OK" },
    { "response", boost::json::value_from(allocate_region{ 69, length }) } };
}
auto handle(FAM::request::mmap_file const)
{
  using namespace FAM::response;
  throw std::runtime_error("not implemented");
  return boost::json::value_from(ping{});
}
auto handle(FAM::request::create_QP const)
{
  using namespace FAM::response;
  throw std::runtime_error("not implemented");
  return boost::json::value_from(ping{});
}


auto handle_rpc(std::string const &req)
{
  using namespace boost::json;
  using namespace FAM;
  auto const msg = parse(req);
  auto const type = string_to_type(value_to<std::string>(msg.at("type")));
  auto const message = msg.at("message");
  switch (type) {
  case request::type::PING:
    return handle(value_to<request::ping>(message));
  case request::type::ALLOCATE_REGION:
    return handle(value_to<request::allocate_region>(message));
  case request::type::MMAP_FILE:
    return handle(value_to<request::mmap_file>(message));
  case request::type::CREATE_QP:
    return handle(value_to<request::create_QP>(message));
  }
  throw std::runtime_error("handle_rpc: unrecognized req");
}

}// namespace

namespace FAM {
namespace request {
  auto tag_invoke(boost::json::value_to_tag<ping>, boost::json::value const &)
  {
    return ping{};
  }

  auto tag_invoke(boost::json::value_to_tag<allocate_region>,
    boost::json::value const &v)
  {
    using namespace boost::json;
    return allocate_region{ value_to<std::uint64_t>(v.at("size")) };
  }

  auto tag_invoke(boost::json::value_to_tag<mmap_file>,
    boost::json::value const &)
  {
    return mmap_file{ std::string("test") };
  }

  auto tag_invoke(boost::json::value_to_tag<create_QP>,
    boost::json::value const &)
  {
    return create_QP{};
  }
}// namespace request
namespace response {
  void tag_invoke(boost::json::value_from_tag,
    boost::json::value &jv,
    ping const &)
  {
    jv = {};
  }

  void tag_invoke(boost::json::value_from_tag,
    boost::json::value &jv,
    allocate_region const &resp)
  {
    jv = { { "addr", resp.addr }, { "length", resp.length } };
  }

}// namespace response
}// namespace FAM

namespace {
void do_session(tcp::socket socket)
{
  try {
    // Construct the stream by moving in the socket
    websocket::stream<tcp::socket> ws{ std::move(socket) };

    // Set a decorator to change the Server of the handshake
    ws.set_option(
      websocket::stream_base::decorator([](websocket::response_type &res) {
        res.set(http::field::server,
          std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-sync");
      }));

    // Accept the websocket handshake
    ws.accept();

    for (;;) {
      // This buffer will hold the incoming message
      beast::flat_buffer buffer;

      // Read a message
      ws.read(buffer);
      spdlog::debug("{}", beast::buffers_to_string(buffer.data()));

      auto const text = boost::json::serialize(
        handle_rpc(beast::buffers_to_string(buffer.data())));

      spdlog::debug(text);

      // Echo the message back
      ws.text(true);
      ws.write(net::buffer(std::string(text)));
    }
  } catch (beast::system_error const &se) {
    // This indicates that the session was closed
    if (se.code() != websocket::error::closed)
      std::cerr << "Error: " << se.code().message() << std::endl;
  } catch (std::exception const &e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}
}// namespace

void FAM::server::run(std::string const &addr, std::string const &port)
{
  spdlog::set_level(spdlog::level::debug);
  auto const address = net::ip::make_address(addr);
  auto const p = static_cast<unsigned short>(std::atoi(port.c_str()));

  // The io_context is required for all I/O
  net::io_context ioc{ 1 };

  // The acceptor receives incoming connections
  tcp::acceptor acceptor{ ioc, { address, p } };
  for (;;) {
    // This will receive the new connection
    tcp::socket socket{ ioc };

    // Block until we get a connection
    acceptor.accept(socket);

    // Launch the session, transferring ownership of the socket
    std::thread(&do_session, std::move(socket)).detach();
  }
}

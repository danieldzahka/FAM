#include <FAM.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

namespace beast = boost::beast;// from <boost/beast.hpp>
namespace http = beast::http;// from <boost/beast/http.hpp>
namespace websocket = beast::websocket;// from <boost/beast/websocket.hpp>
namespace net = boost::asio;// from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;// from <boost/asio/ip/tcp.hpp>

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

      // Echo the message back
      ws.text(ws.got_text());
      ws.write(buffer.data());
      std::cout << "got message\n";
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

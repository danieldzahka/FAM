#include <FAM.hpp>
#include <stdexcept>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

// namespace beast = boost::beast;         // from <boost/beast.hpp>
// namespace http = beast::http;           // from <boost/beast/http.hpp>
// namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
// namespace net = boost::asio;            // from <boost/asio.hpp>
// using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

using namespace FAM::client;

RPC_client::RPC_client(std::string host, std::string port)
{
  auto const text = "This is a test message";
  // Look up the domain name
  auto const results = resolver.resolve(host, port);

  // Make the connection on the IP address we get from a lookup
  auto ep = net::connect(ws.next_layer(), results);

  // Update the host_ string. This will provide the value of the
  // Host HTTP header during the WebSocket handshake.
  // See https://tools.ietf.org/html/rfc7230#section-5.4
  host += ':' + std::to_string(ep.port());

  // Set a decorator to change the User-Agent of the handshake
  ws.set_option(
    websocket::stream_base::decorator([](websocket::request_type &req) {
      req.set(http::field::user_agent,
        std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
    }));

  // Perform the websocket handshake
  ws.handshake(host, "/");

  // Send the message
  ws.write(net::buffer(std::string(text)));

  // This buffer will hold the incoming message
  beast::flat_buffer buffer;

  // Read a message into our buffer
  ws.read(buffer);

  // Close the WebSocket connection
  ws.close(websocket::close_code::normal);

  // If we get here then the connection is closed gracefully

  // The make_printable() function helps print a ConstBufferSequence
  std::cout << beast::make_printable(buffer.data()) << std::endl;
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

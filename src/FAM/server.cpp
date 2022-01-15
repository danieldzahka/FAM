#include <FAM.hpp>
#include "FAM_rdma.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/json.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include <poll.h>

#include <spdlog/spdlog.h>//delete maybe

namespace beast = boost::beast;// from <boost/beast.hpp>
namespace http = beast::http;// from <boost/beast/http.hpp>
namespace websocket = beast::websocket;// from <boost/beast/websocket.hpp>
namespace net = boost::asio;// from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;// from <boost/asio/ip/tcp.hpp>

namespace {
ibv_pd *volatile my_pd;

class session
{
public:
  rdma_cm_id *const id;
  std::vector<FAM::RDMA::RDMA_mem> client_regions;
  session(rdma_cm_id *const t_id) : id{ t_id } {}

  session &operator=(const session &) = delete;
  session(const session &) = delete;
};
}// namespace

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
auto handle(FAM::request::allocate_region const req, session &s)
{
  using namespace FAM::response;
  auto const length = req.size;
  try {
    s.client_regions.emplace_back(s.id, length, false, true);
    auto ptr = reinterpret_cast<uint64_t>(s.client_regions.back().p.get());
    return boost::json::value{ { "status", "OK" },
      { "response", boost::json::value_from(allocate_region{ ptr, length }) } };
  } catch (std::exception const &e) {
    return boost::json::value{ { "status", "OK" }, { "response", e.what() } };
  }

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


auto handle_rpc(std::string const &req, session &s)
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
    return handle(value_to<request::allocate_region>(message), s);
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
void do_session(tcp::socket socket, rdma_cm_id *const id)
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

    id->pd = my_pd;
    session s{ id };

    for (;;) {
      beast::flat_buffer buffer;
      ws.read(buffer);
      spdlog::debug("{}", beast::buffers_to_string(buffer.data()));

      auto const text = boost::json::serialize(
        handle_rpc(beast::buffers_to_string(buffer.data()), s));

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

void rdma_server(rdma_event_channel *const ec)
{
  spdlog::debug("Running RDMA server...");
  struct rdma_cm_event *event = NULL;

  while (1) {
    pollfd pfd;
    nfds_t const n_fds = 1;
    int const ms = 500;
    pfd.fd = ec->fd;
    pfd.events = POLLIN | POLLOUT | POLLPRI;
    auto ret = poll(&pfd, n_fds, ms);
    if (ret == -1) {
      spdlog::error("poll error -1");
      throw std::runtime_error("poll returned -1");
    } else if (ret > 0) {
      if (rdma_get_cm_event(ec, &event) == 0) {
        struct rdma_cm_event event_copy;
        memcpy(&event_copy, event, sizeof(*event));
        rdma_ack_cm_event(event);

        spdlog::debug(rdma_event_str(event_copy.event));
        
        if (event_copy.event
            == RDMA_CM_EVENT_CONNECT_REQUEST) {// Runs on server
          struct ibv_qp_init_attr qp_attr;
          memset(&qp_attr, 0, sizeof(qp_attr));
          qp_attr.qp_type = IBV_QPT_RC;
          qp_attr.cap.max_send_wr = 10000;// increase later
          qp_attr.cap.max_recv_wr = 10;
          qp_attr.cap.max_send_sge = 1;
          qp_attr.cap.max_recv_sge = 1;
          qp_attr.sq_sig_all = 0;// shouldn't need this explicitly
          if (rdma_create_qp(event_copy.id, nullptr, &qp_attr))
            throw std::runtime_error("rdma_create_qp() failed!");

          auto params = FAM::RDMA::get_cm_params();
          auto const err = rdma_accept(event_copy.id, &params);
          if (err) spdlog::error("rdma_accept() failed!");
        } else if (event_copy.event
                   == RDMA_CM_EVENT_ESTABLISHED) {// Runs on both
          my_pd = event_copy.id->pd; //grab this if null in session
          // spdlog::debug("id {} id->verbs {} id->pd {}",
          // (void*)(event_copy.id), (void*)(event_copy.id)->verbs,
          // (void*)(event_copy.id)->pd); FAM::RDMA::RDMA_mem m {event_copy.id,
          // 69, false, false};
        } else if (event_copy.event
                   == RDMA_CM_EVENT_DISCONNECTED) {// Runs on both
          rdma_disconnect(event_copy.id);
          rdma_destroy_qp(event_copy.id);
          rdma_destroy_id(event_copy.id);
        }
      } else {
        spdlog::error("get cm event error");
        throw std::runtime_error("rdma get cm event");
      }
    } else {
      spdlog::debug("woke up. no data");
    }
  }
}
}// namespace

void FAM::server::run(std::string const &host, std::string const &port)
{
  spdlog::set_level(spdlog::level::debug);
  auto const address = net::ip::make_address(host);
  auto const p = static_cast<unsigned short>(std::atoi(port.c_str()));

  auto ec = FAM::RDMA::create_ec();
  auto id = FAM::RDMA::create_id(ec.get());
  FAM::RDMA::bind_addr(id.get());
  FAM::RDMA::listen(id.get());
  auto const rdma_port = ntohs(rdma_get_src_port(id.get()));
  auto addr = rdma_get_local_addr(id.get());
  char *ip = inet_ntoa(reinterpret_cast<sockaddr_in *>(addr)->sin_addr);
  spdlog::debug("Server listening on IPoIB: {}:{}", ip, rdma_port);
  std::thread(rdma_server, ec.get()).detach();

  net::io_context ioc{ 1 };
  tcp::acceptor acceptor{ ioc, { address, p } };

  for (;;) {
    tcp::socket socket{ ioc };
    acceptor.accept(socket);

    // std::thread(rdma_server, ec.get()).detach();
    std::thread(&do_session, std::move(socket), id.get()).detach();
  }
}

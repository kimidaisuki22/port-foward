#include <asio.hpp>
#include <asio/io_context.hpp>
#include <asio/registered_buffer.hpp>
#include <asio/write.hpp>
#include <exception>
#include <fmt/format.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace asio;
using ip::tcp;
using std::vector;

void pipe(tcp::socket &src_socket, tcp::socket &dest_socket) {
  constexpr size_t buffer_size = 1024 * 64;
  char data[buffer_size];
  try {
    while (true) {
      size_t length = src_socket.read_some(buffer(data, buffer_size));
      asio::write(dest_socket, buffer(data, length));
      // std::cout << std::string_view{data, length};
    }
  } catch (std::exception &e) {
    SPDLOG_ERROR("Exception in pipe: {}", e.what());
  }
}

// Function to forward data between two sockets
void forward(tcp::socket src_socket, tcp::socket dest_socket) {
  std::thread t1{[&] { pipe(src_socket, dest_socket); }};
  pipe(dest_socket, src_socket);

  t1.join();

  SPDLOG_INFO("forward closed.");
}

// Function to accept client connections and create forwarder threads
void accept_clients(tcp::acceptor &acceptor, tcp::endpoint dest_endpoint) {
  vector<std::thread> forwarder_threads;

  try {
    while (true) {
      tcp::socket client_socket(acceptor.get_executor());
      acceptor.accept(client_socket);
      auto remote = client_socket.remote_endpoint();
      SPDLOG_INFO("new connection from {}:{}", remote.address().to_string(),
                  remote.port());

      // Create a destination socket and connect
      tcp::socket dest_socket(acceptor.get_executor());
      dest_socket.connect(dest_endpoint);

      // Create a forwarder thread
      forwarder_threads.emplace_back(forward, std::move(client_socket),
                                     std::move(dest_socket));
    }
  } catch (std::exception &e) {
    SPDLOG_CRITICAL("Exception in accept clients: {}", e.what());
  }

  // Wait for all forwarder threads to finish
  for (auto &thread : forwarder_threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}
struct Forward_item {
  std::string listen_address_;
  std::string dest_address_;

  uint16_t listen_port_;
  uint16_t dest_port_;
};
std::string to_string(const Forward_item &item) {
  return fmt::format("forward from {}:{} -> {}:{}", item.listen_address_,
                     item.listen_port_, item.dest_address_, item.dest_port_);
}
int main(int argc, char **argv) {
  if (argc < 5) {
    SPDLOG_INFO("Usage: (listen ip) (port) (dest address) (port)");
    exit(1);
  }
  try {
    Forward_item item(argv[1], argv[3], std::stoi(argv[2]), std::stoi(argv[4]));
    io_context io;

    // Define source and destination endpoints
    tcp::endpoint source_endpoint(
        ip::address::from_string(item.listen_address_),
        item.listen_port_); // Change as needed
    tcp::endpoint dest_endpoint(ip::address::from_string(item.dest_address_),
                                item.dest_port_); // Change as needed

    // Create an acceptor for clients
    tcp::acceptor acceptor(io, source_endpoint);

    SPDLOG_INFO("start forward: {}", to_string(item));
    // Start accepting clients and forwarding data
    accept_clients(acceptor, dest_endpoint);
  } catch (std::exception &e) {
    SPDLOG_ERROR("Exception in main: {}", e.what());
  }

  return 0;
}

#include <asio.hpp>
#include <asio/io_context.hpp>
#include <asio/registered_buffer.hpp>
#include <asio/write.hpp>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace asio;
using ip::tcp;
using std::cerr;
using std::cout;
using std::endl;
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

    cerr << "Exception in forward: " << e.what() << endl;
  }
}

// Function to forward data between two sockets
void forward(tcp::socket src_socket, tcp::socket dest_socket) {
  try {
    std::thread t1{[&] { pipe(src_socket, dest_socket); }};
    pipe(dest_socket, src_socket);

    t1.join();
  } catch (std::exception &e) {
    cerr << "Exception in forward: " << e.what() << endl;
  }
}

// Function to accept client connections and create forwarder threads
void accept_clients(tcp::acceptor &acceptor, tcp::endpoint dest_endpoint) {
  vector<std::thread> forwarder_threads;

  try {
    while (true) {
      tcp::socket client_socket(acceptor.get_executor());
      acceptor.accept(client_socket);
      std::cout << "new connection from "
                << client_socket.remote_endpoint().address().to_string() << ":"
                << client_socket.remote_endpoint().port() << "\n";

      // Create a destination socket and connect
      tcp::socket dest_socket(acceptor.get_executor());
      dest_socket.connect(dest_endpoint);

      // Create a forwarder thread
      forwarder_threads.emplace_back(forward, std::move(client_socket),
                                     std::move(dest_socket));
    }
  } catch (std::exception &e) {
    cerr << "Exception in accept_clients: " << e.what() << endl;
  }

  // Wait for all forwarder threads to finish
  for (auto &thread : forwarder_threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

int main(int argc, char **argv) {
  if (argc < 5) {
    std::cerr << "Usage "
              << "'dest' 'port' <- 'listen ip' 'port'\n";
    exit(1);
  }
  try {
    io_context io;

    // Define source and destination endpoints
    tcp::endpoint source_endpoint(ip::address::from_string(argv[3]),
                                  std::stoi(argv[4])); // Change as needed
    tcp::endpoint dest_endpoint(ip::address::from_string(argv[1]),
                                std::stoi(argv[2])); // Change as needed

    // Create an acceptor for clients
    tcp::acceptor acceptor(io, source_endpoint);

    // Start accepting clients and forwarding data
    accept_clients(acceptor, dest_endpoint);
  } catch (std::exception &e) {
    cerr << "Exception in main: " << e.what() << endl;
  }

  return 0;
}

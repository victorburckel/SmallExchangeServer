#include "epoll_impl.h"
#include "exchange_server.h"
#include "scope_exit.h"
#include "socket_impl.h"
#include "worker.h"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <functional>
#include <memory>
#include <sys/eventfd.h>
#include <thread>

// This file will be generated automatically when you run the CMake configuration step.
// It creates a namespace called `SmallExchangeServer`.
// You can modify the source template at `configured_files/config.hpp.in`.
#include <internal_use_only/config.hpp>

int main(int argc, const char **argv)
{
  try
  {
    CLI::App app{ fmt::format(
      "{} version {}", SmallExchangeServer::cmake::project_name, SmallExchangeServer::cmake::project_version) };

    int port{ 9090 };
    app.add_option("-p,--port", port, "Port number to listen");
    bool show_version = false;
    app.add_flag("--version", show_version, "Show version information");

    CLI11_PARSE(app, argc, argv);

    if (show_version)
    {
      fmt::print("{}\n", SmallExchangeServer::cmake::project_version);
      return EXIT_SUCCESS;
    }

    spdlog::info("Starting server on port {}", port);

    auto worker = std::make_shared<exchange_server::worker>();

    exchange_server::server server{ std::make_shared<exchange_server::listen_socket_impl>(port),
      std::make_shared<exchange_server::epoll_impl>(),
      worker,
      std::make_shared<exchange_server::socket_impl>(eventfd(0, 0)) };

    // Should use jthread
    std::thread runner{ [worker] { worker->run(); } };
    exchange_server::scope_exit guard{ [&runner, worker]() {
      worker->stop();
      runner.join();
    } };

    server.run();

    spdlog::info("Closing server");
  } catch (const std::exception &e)
  {
    spdlog::error("Unhandled exception in main: {}", e.what());
  }
}

#include "exchange_server.h"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <functional>
#include <iostream>

// This file will be generated automatically when you run the CMake configuration step.
// It creates a namespace called `SmallExchangeServer`.
// You can modify the source template at `configured_files/config.hpp.in`.
#include <internal_use_only/config.hpp>


int main(int argc, const char **argv)
{
  try {
    CLI::App app{ fmt::format("{} version {}", SmallExchangeServer::cmake::project_name, SmallExchangeServer::cmake::project_version) };

    int port{ 9090 };
    app.add_option("-p,--port", port, "Port number to listen");
    bool show_version = false;
    app.add_flag("--version", show_version, "Show version information");

    CLI11_PARSE(app, argc, argv);

    if (show_version) {
      fmt::print("{}\n", SmallExchangeServer::cmake::project_version);
      return EXIT_SUCCESS;
    }

    // Use the default logger (stdout, multi-threaded, colored)
    spdlog::info("Starting server on port {}", port);

    exchange_server::server server{ port };

    server.run();

    spdlog::info("Closing server");
  } catch (const std::exception &e) {
    spdlog::error("Unhandled exception in main: {}", e.what());
  }
}

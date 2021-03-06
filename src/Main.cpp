#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "afk/Afk.hpp"

using std::exception;

auto main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) -> int {
  auto &afk = Afk::Engine::get();

  afk.initialize();

  while (afk.get_is_running()) {
    afk.update();
    afk.render();
  }

  return EXIT_SUCCESS;
}

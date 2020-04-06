#include <cstdlib>
#include <iostream>

#include "afk/Afk.hpp"
#include "afk/renderer/ShaderProgram.hpp"

auto main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) -> int {
  auto &afk = Afk::Engine::get();

  while (afk.get_is_running()) {
    afk.update();
    afk.render();
  }

  return EXIT_SUCCESS;
}

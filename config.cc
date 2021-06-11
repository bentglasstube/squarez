#include "config.h"

Config::Config() : Game::Config() {
  graphics.title = "Squarez";
  graphics.width = 2560;
  graphics.height = 1440;
  graphics.intscale = 2;
  graphics.fullscreen = true;
}

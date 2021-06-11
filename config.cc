#include "config.h"

Config::Config() : Game::Config() {
  graphics.title = "Squarez";
  graphics.width = 1280;
  graphics.height = 720;
  graphics.intscale = 2;
  graphics.fullscreen = false;
}

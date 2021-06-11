#pragma once

#include <random>

#include "entt/entity/registry.hpp"

#include "screen.h"
#include "text.h"

#include "geometry.h"

class GameScreen : public Screen {
  public:

    GameScreen();

    bool update(const Input& input, Audio& audio, unsigned int elapsed) override;
    void draw(Graphics& graphics) const override;

  private:

    enum class state { playing, paused, won, lost };

    entt::registry reg_;
    std::mt19937 rng_;
    Text text_;

    state state_;
    int score_;

    void add_box(size_t count = 1);
    void explosion(const pos p, uint32_t color);

    void user_input(const Input& input);
    void collision();
    void cleanup();
    void particles(float t);
    void movement(float t);
    void fading(float t);
    void firing(float t);
    void flocking();

    void stay_in_bounds();
};

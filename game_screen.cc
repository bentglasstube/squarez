#include "game_screen.h"

#include "util.h"

#include "components.h"
#include "config.h"

GameScreen::GameScreen() : rng_(Util::random_seed()), text_("text.png"), state_(state::playing), score_(0) {
  const auto player = reg_.create();
  reg_.emplace<Color>(player, 0xd8ff00ff);
  reg_.emplace<Position>(player, pos{ kConfig.graphics.width / 2.0f, kConfig.graphics.height / 2.0f});
  reg_.emplace<PlayerControl>(player);
  reg_.emplace<Collision>(player);
  reg_.emplace<ScreenWrap>(player);
  reg_.emplace<Accelleration>(player);
  reg_.emplace<Velocity>(player, 0.0f);
  reg_.emplace<Angle>(player, 0.0f);
  reg_.emplace<Rotation>(player);
  reg_.emplace<Size>(player, 20.0f);
  reg_.emplace<Health>(player, 1000);

  add_box(100);
}

bool GameScreen::update(const Input& input, Audio&, unsigned int elapsed) {
  const float t = elapsed / 1000.0f;
  expiring(t);

  switch (state_) {
    case state::playing:
      if (input.key_pressed(Input::Button::Start)) {
        state_ = state::paused;
      }

      user_input(input);

      // movement systems
      accelleration(t);
      rotation(t);
      steering(t);
      flocking();
      stay_in_bounds();
      max_velocity();
      movement(t);

      // state systems
      firing(t);

      // collision systems
      collision();

      // cleanup systems
      kill_dead();
      kill_oob();

      if (reg_.view<PlayerControl>().size() == 0) {
        state_ = state::lost;

        const auto fade = reg_.create();
        reg_.emplace<FadeOut>(fade);
        reg_.emplace<Timer>(fade, 2.5f, false);
        reg_.emplace<Color>(fade, 0x000000ff);
      }

      break;

    case state::paused:
      if (input.key_pressed(Input::Button::Start)) {
        state_ = state::playing;
      }
      break;

    default:
      // do nothing
      break;

  }

  return true;
}

namespace {
  rect get_rect(const pos& p, float size) {
    return { p.x - size / 2, p.y - size / 2, p.x + size / 2, p.y + size / 2 };
  }

  uint32_t color_opacity(uint32_t color, float opacity) {
    const uint32_t lsb = (uint32_t)((color & 0xff) * std::clamp(opacity, 0.0f, 1.0f));
    return (color & 0xffffff00) | lsb;
  }
}

void GameScreen::draw(Graphics& graphics) const {
  draw_flash(graphics);
  draw_particles(graphics);
  draw_squares(graphics);
  draw_bullets(graphics);
  draw_overlay(graphics);
}

void GameScreen::draw_flash(Graphics& graphics) const {
  const auto flashes = reg_.view<const Flash, const Timer, const Color>();
  for (const auto f : flashes) {
    const uint32_t c = color_opacity(flashes.get<const Color>(f).color, 1 - (flashes.get<const Timer>(f).ratio()));
    graphics.draw_rect({0, 0}, {graphics.width(), graphics.height()}, c, true);
  }
}

void GameScreen::draw_particles(Graphics& graphics) const {
  const auto particles = reg_.view<const Particle, const Timer, const Position, const Color>();
  for (const auto pt : particles) {
    const pos p = particles.get<const Position>(pt).p;
    graphics.draw_pixel({ (int)p.x, (int)p.y }, color_opacity(particles.get<const Color>(pt).color, 1 - particles.get<const Timer>(pt).ratio()));
  }
}

void GameScreen::draw_squares(Graphics& graphics) const {
  const auto squarez = reg_.view<const Position, const Size, const Color, const Angle>();
  for (const auto s : squarez) {
    const pos p = squarez.get<const Position>(s).p;
    const float size = squarez.get<const Size>(s).size;
    const rect r = get_rect(p, size);
    const bool filled = reg_.all_of<PlayerControl>(s);

    graphics.draw_rect({ (int)r.left, (int)r.top }, { (int)r.right, (int)r.bottom }, squarez.get<const Color>(s).color, filled);
    const float angle = reg_.get<const Angle>(s).angle;
    graphics.draw_line({ (int)p.x, (int)p.y }, { (int)(p.x + size * std::cos(angle)), (int)(p.y + size * std::sin(angle)) }, 0xffffffff);
  }
}

void GameScreen::draw_bullets(Graphics& graphics) const {
  const auto bullets = reg_.view<const Position, const Bullet>();
  for (const auto b : bullets) {
    const pos p = bullets.get<const Position>(b).p;
    graphics.draw_circle({ (int)p.x, (int)p.y }, 2, 0xffffffff, true);
  }
}

namespace {
  void text_box(Graphics& graphics, const Text& text, const std::string& msg) {
    static const int width = 50;
    static const int height = 20;

    const Graphics::Point p1 { graphics.width() / 2 - width, graphics.height() / 2 - height };
    const Graphics::Point p2 { graphics.width() / 2 + width, graphics.height() / 2 + height };

    graphics.draw_rect(p1, p2, 0x000000ff, true);
    graphics.draw_rect(p1, p2, 0xffffffff, false);
    text.draw(graphics, msg, graphics.width() / 2, graphics.height() / 2 - 8, Text::Alignment::Center);
  }

  void health_box(Graphics& graphics, const Graphics::Point p1, const Graphics::Point p2, uint32_t color, float fullness) {
    graphics.draw_rect(p1, p2, 0x000000ff, true);
    graphics.draw_rect(p1, { p1.x + (int)((p2.x - p1.x) * fullness), p2.y }, color, true);
    graphics.draw_rect(p1, p2, color, false);
  }
}

void GameScreen::draw_overlay(Graphics& graphics) const {
  const auto fade = reg_.view<const FadeOut, const Timer, const Color>();
  for (const auto f : fade) {
    const uint32_t c = color_opacity(fade.get<const Color>(f).color, fade.get<const Timer>(f).ratio());
    graphics.draw_rect({0, 0}, {graphics.width(), graphics.height()}, c, true);
  }

  if (state_ == state::paused) {
    graphics.draw_rect({0, 0}, {graphics.width(), graphics.height()}, 0x00000099, true);
    text_box(graphics, text_, "Paused");
  } else if (state_ == state::lost) {
    text_box(graphics, text_, "Game Over");
  }

  text_.draw(graphics, std::to_string(score_), graphics.width(), 0, Text::Alignment::Right);

  // TODO make work for multiple players
  const auto players = reg_.view<const PlayerControl, const Color, const Health>();
  for (const auto p : players) {
    const Graphics::Point start {0, graphics.height() - 16};
    const Graphics::Point end {graphics.width(), graphics.height()};
    health_box(graphics, start, end, players.get<const Color>(p).color, players.get<const Health>(p).health / 1000.0f);
  }
}

void GameScreen::add_box(size_t count) {
  std::uniform_real_distribution<float> hue(0, 260);
  std::uniform_int_distribution<int> size(10, 20);
  std::uniform_int_distribution<int> px(0, kConfig.graphics.width);
  std::uniform_int_distribution<int> py(0, kConfig.graphics.height);
  std::uniform_real_distribution<float> angle(0, 2 * M_PI);
  std::uniform_real_distribution<float> velocity(1, 5);

  for (size_t i = 0; i < count; ++i) {
    const auto square = reg_.create();
    const uint32_t c = hsl{hue(rng_), 1.0f, 0.5f};
    const pos p = { (float)px(rng_), (float)py(rng_) };

    reg_.emplace<Health>(square, 1);
    reg_.emplace<Color>(square, c);
    reg_.emplace<Position>(square, p);
    reg_.emplace<Size>(square, size(rng_));
    reg_.emplace<Collision>(square);
    reg_.emplace<Velocity>(square, velocity(rng_));
    reg_.emplace<Angle>(square, angle(rng_));
    reg_.emplace<MaxVelocity>(square);
    reg_.emplace<Flocking>(square);
    reg_.emplace<StayInBounds>(square);
  }
}

void GameScreen::explosion(const pos p, uint32_t color) {
  std::uniform_real_distribution<float> angle(0, 2 * M_PI);
  std::uniform_real_distribution<float> velocity(1, 15);
  std::uniform_real_distribution<float> lifetime(1.5f, 4.5f);

  for (size_t i = 0; i < 500; ++i) {
    const auto pt = reg_.create();

    reg_.emplace<Particle>(pt);
    reg_.emplace<Timer>(pt, lifetime(rng_));
    reg_.emplace<Position>(pt, p);
    reg_.emplace<Color>(pt, color);
    reg_.emplace<Velocity>(pt, velocity(rng_));
    reg_.emplace<Angle>(pt, angle(rng_));
    reg_.emplace<StayInBounds>(pt);
  }
}

void GameScreen::user_input(const Input& input) {
  auto view = reg_.view<const PlayerControl, Accelleration, Rotation>();
  for (auto e : view) {
    float& accel = view.get<Accelleration>(e).accel;
    float& rot = view.get<Rotation>(e).rot;

    accel = 0.0f;
    if (input.key_held(Input::Button::Up)) accel += 10.0f;
    if (input.key_held(Input::Button::Down)) accel -= 2.0f;

    rot = 0.0f;
    if (input.key_held(Input::Button::Left)) rot -= 1.0f;
    if (input.key_held(Input::Button::Right)) rot += 1.0f;

    if (input.key_held(Input::Button::A)) {
      static_cast<void>(reg_.get_or_emplace<Firing>(e));
    } else {
      reg_.remove<Firing>(e);
    }
  }
}

void GameScreen::collision() {
  auto players = reg_.view<const PlayerControl, const Position, const Size, Health>();
  for (auto player : players) {
    const rect player_rect = get_rect(players.get<const Position>(player).p, players.get<const Size>(player).size);
    auto targets = reg_.view<const Collision, const Position, const Size>();
    for (auto t : targets) {
      if (player == t) continue;

      const rect r = get_rect(targets.get<const Position>(t).p, targets.get<const Size>(t).size);

      if (r.intersect(player_rect)) {
        players.get<Health>(player).health--;

        const auto flash = reg_.create();
        reg_.emplace<Flash>(flash);
        reg_.emplace<Timer>(flash, 0.2f);
        reg_.emplace<Color>(flash, 0x77000033);

        reg_.destroy(t);
        add_box();
      }
    }
  }

  auto bullets = reg_.view<const Bullet, const Position>();
  for (auto b : bullets) {
    const pos p = bullets.get<const Position>(b).p;
    auto targets = reg_.view<const Collision, const Position, const Size, Health>();
    for (auto t : targets) {
      if (t == bullets.get<const Bullet>(b).source) continue;

      const rect r = get_rect(targets.get<const Position>(t).p, targets.get<const Size>(t).size);

      if (r.contains(p)) {
        targets.get<Health>(t).health--;
        reg_.destroy(b);
        break;
      }
    }
  }
}

void GameScreen::kill_dead() {
  auto view = reg_.view<const Health, const Position, const Color>();
  for (const auto e : view) {
    if (view.get<const Health>(e).health <= 0.0f) {
      const uint32_t color = view.get<const Color>(e).color;
      const pos p = view.get<const Position>(e).p;
      explosion(p, color);
      ++score_;

      reg_.destroy(e);
      add_box();
    }
  }
}

namespace {
  constexpr bool oob(pos p) {
    if (p.x < 0 || p.x > kConfig.graphics.width) return true;
    if (p.y < 0 || p.y > kConfig.graphics.height) return true;
    return false;
  }
}

void GameScreen::kill_oob() {
  auto view = reg_.view<const Position, const KillOffScreen>();
  for (const auto e : view) {
    if (oob(view.get<const Position>(e).p)) reg_.destroy(e);
  }
}

void GameScreen::accelleration(float t) {
  auto view = reg_.view<Velocity, const Accelleration>();
  for (const auto e : view) {
    float& vel = view.get<Velocity>(e).vel;
    vel = (vel + view.get<const Accelleration>(e).accel * t) * 0.99;
  }
}

void GameScreen::rotation(float t) {
  auto view = reg_.view<Angle, const Rotation>();
  for (const auto e : view) {
    float& angle = view.get<Angle>(e).angle;
    angle += view.get<const Rotation>(e).rot * t;
  }
}

void GameScreen::steering(float t) {
  auto view = reg_.view<Angle, const TargetDir>();
  for (const auto e : view) {
    float& angle = view.get<Angle>(e).angle;
    angle += std::clamp(view.get<const TargetDir>(e).target - angle, -t, t);
  }
}

void GameScreen::max_velocity() {
  auto view = reg_.view<Velocity, const MaxVelocity>();
  for (const auto e : view) {
    float& vel = view.get<Velocity>(e).vel;
    const float max = view.get<const MaxVelocity>(e).max;
    if (vel > max) vel = max;
  }
}

void GameScreen::movement(float t) {
  auto view = reg_.view<Position, const Velocity, const Angle>();
  for (const auto e : view) {
    pos& p = view.get<Position>(e).p;
    const float vel = view.get<const Velocity>(e).vel;
    const float angle = view.get<const Angle>(e).angle;

    p += pos::polar(vel, angle);

    if (reg_.all_of<ScreenWrap>(e)) {
      while (p.x < 0) p.x += kConfig.graphics.width;
      while (p.x > kConfig.graphics.width) p.x -= kConfig.graphics.width;
      while (p.y < 0) p.y += kConfig.graphics.height;
      while (p.y > kConfig.graphics.height) p.y -= kConfig.graphics.height;
    }
  }
}

void GameScreen::expiring(float t) {
  auto view = reg_.view<Timer>();
  for (const auto e : view) {
    Timer& tm = view.get<Timer>(e);
    tm.elapsed += t;
    if (tm.expire && tm.elapsed > tm.lifetime) reg_.destroy(e);
  }
}

void GameScreen::firing(float t) {
  auto view = reg_.view<Firing, const Position, const Angle>();
  for (const auto e : view) {
    Firing& gun = view.get<Firing>(e);

    gun.time += t;
    if (gun.time > gun.rate) {
      gun.time -= gun.rate;
      const pos p = view.get<const Position>(e).p;
      const float a = view.get<const Angle>(e).angle;

      const auto bullet = reg_.create();
      reg_.emplace<Bullet>(bullet, e);
      reg_.emplace<Position>(bullet, pos{p.x + 5 * std::cos(a), p.y + 5 * std::sin(a)});
      reg_.emplace<Velocity>(bullet, 13);
      reg_.emplace<Angle>(bullet, a);
    }
  }
}

void GameScreen::flocking() {
  auto view = reg_.view<const Flocking, const Position, Velocity, const Angle>();
  for (const auto e : view) {
    const pos boid = view.get<const Position>(e).p;
    const float angle = view.get<const Angle>(e).angle;
    float& vel = view.get<Velocity>(e).vel;

    int count = 0;
    pos center, flock, avoid;
    pos v = pos::polar(vel, angle);

    auto nearby = reg_.view<const Flocking, const Position, const Velocity, const Angle>();
    for (const auto other : nearby) {
      if (other == e) continue;

      pos p = nearby.get<const Position>(other).p;
      float d = p.dist2(boid);

      // close enough to see
      if (d < 75.0f * 75.0f) {
        ++count;
        center += p;
        flock += pos::polar(nearby.get<const Velocity>(other).vel, nearby.get<const Angle>(other).angle);
      }

      // too close
      if (d < 20.0f * 20.0f) avoid += boid - p;
    }

    if (count > 0) {
      center /= count;
      flock /= count;

      const pos delta = (center - boid) * 0.005f + avoid * 0.05f + flock * 0.05f;
      v += delta;

      reg_.emplace_or_replace<TargetDir>(e, v.angle());
      vel = v.mag();
    }
  }
}

void GameScreen::stay_in_bounds() {
  const float buffer = 25.0f;

  auto view = reg_.view<const StayInBounds, const Position, Velocity, Angle>();
  for (const auto e : view) {
    const pos p = view.get<const Position>(e).p;
    float& vel = view.get<Velocity>(e).vel;
    float& angle = view.get<Angle>(e).angle;

    pos v = pos::polar(vel, angle);

    if (p.x < buffer) v.x += 1.0f;
    if (p.x > kConfig.graphics.width - buffer) v.x -= 1.0f;
    if (p.y < buffer) v.y += 1.0f;
    if (p.y > kConfig.graphics.height - buffer) v.y -= 1.0f;

    vel = v.mag();
    angle = v.angle();
  }
}

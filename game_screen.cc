#include "game_screen.h"

#include "util.h"

#include "components.h"
#include "config.h"
#include "geometry.h"

GameScreen::GameScreen() : rng_(Util::random_seed()), state_(state::playing) {
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
  reg_.emplace<Health>(player);

  add_box(50);
}

bool GameScreen::update(const Input& input, Audio&, unsigned int elapsed) {
  const float t = elapsed / 1000.0f;
  fading(t);

  switch (state_) {
    case state::playing:
      if (input.key_pressed(Input::Button::Start)) {
        state_ = state::paused;
      }

      user_input(input);

      movement(t);
      firing(t);
      flocking();
      stay_in_bounds();

      collision();
      cleanup();

      if (reg_.view<PlayerControl>().size() == 0) {
        state_ = state::lost;
        const auto fade = reg_.create();
        reg_.emplace<Color>(fade, 0x333333ff);
        reg_.emplace<Fade>(fade, 1.0, Fade::in);
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

  uint32_t fade_color(const Fade& fade, uint32_t color) {
    const float alpha = std::min(fade.elapsed / fade.time, 1.0f);
    const uint32_t lsb = (uint32_t)(255 * (fade.dir == Fade::out ? 1 - alpha : alpha));

    return (color & 0xffffff00) | lsb;
  }
}

void GameScreen::draw(Graphics& graphics) const {
  const auto fades = reg_.view<const Fade, const Color>();
  for (const auto f : fades) {
    const uint32_t c = fade_color(fades.get<const Fade>(f), fades.get<const Color>(f).color);
    graphics.draw_rect({0, 0}, {graphics.width(), graphics.height()}, c, true);
  }

  const auto squarez = reg_.view<const Position, const Size, const Color>();
  for (const auto s : squarez) {
    const pos p = squarez.get<const Position>(s).p;
    const float size = squarez.get<const Size>(s).size;
    const rect r = get_rect(p, size);
    const bool filled = reg_.all_of<PlayerControl>(s);

    graphics.draw_rect({ (int)r.left, (int)r.top }, { (int)r.right, (int)r.bottom }, squarez.get<const Color>(s).color, filled);
    if (reg_.all_of<Firing>(s)) graphics.draw_rect({ (int)r.left, (int)r.top }, { (int)r.right, (int)r.bottom }, 0xff0000ff, false);

    if (reg_.all_of<Angle>(s)) {
      const float angle = reg_.get<Angle>(s).angle;
      graphics.draw_line({ (int)p.x, (int)p.y }, { (int)(p.x + size * std::cos(angle)), (int)(p.y + size * std::sin(angle)) }, 0xffffffff);
    }
  }

  const auto bullets = reg_.view<const Position, const Bullet>();
  for (const auto b : bullets) {
    const pos p = bullets.get<const Position>(b).p;
    graphics.draw_pixel({ (int)p.x, (int)p.y }, 0xd8ff00ff);
  }
}

void GameScreen::add_box(size_t count) {
  std::uniform_int_distribution<uint32_t> color(50, 150);
  std::uniform_int_distribution<int> size(10, 20);
  std::uniform_int_distribution<int> px(0, kConfig.graphics.width);
  std::uniform_int_distribution<int> py(0, kConfig.graphics.height);
  std::uniform_real_distribution<float> angle(0, 2 * M_PI);
  std::uniform_real_distribution<float> velocity(1, 5);

  for (size_t i = 0; i < count; ++i) {
    const auto square = reg_.create();
    const uint32_t c = color(rng_) << 24 | color(rng_) << 16 | color(rng_) << 8 | 0xff;
    const pos p = { (float)px(rng_), (float)py(rng_) };

    reg_.emplace<Health>(square, 5);
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

void GameScreen::user_input(const Input& input) {
  auto view = reg_.view<PlayerControl, Accelleration, Rotation>();
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
  auto players = reg_.view<PlayerControl, Position, Size, Health>();
  for (auto player : players) {
    const rect player_rect = get_rect(players.get<Position>(player).p, players.get<Size>(player).size);
    auto targets = reg_.view<Collision, Position, Size>();
    for (auto t : targets) {
      if (player == t) continue;

      const rect r = get_rect(targets.get<Position>(t).p, targets.get<Size>(t).size);

      if (r.intersect(player_rect)) {
        players.get<Health>(player).health--;

        const auto flash = reg_.create();
        reg_.emplace<Fade>(flash, 0.2f, Fade::out);
        reg_.emplace<Color>(flash, 0x770000ff);

        reg_.destroy(t);
        add_box();
      }
    }
  }

  auto bullets = reg_.view<Bullet, Position>();
  for (auto b : bullets) {
    const pos p = bullets.get<Position>(b).p;
    auto targets = reg_.view<Collision, Position, Size, Health>();
    for (auto t : targets) {
      if (t == bullets.get<Bullet>(b).source) continue;

      const rect r = get_rect(targets.get<Position>(t).p, targets.get<Size>(t).size);

      if (r.contains(p)) {
        targets.get<Health>(t).health--;
        reg_.destroy(b);
        break;
      }
    }
  }
}

void GameScreen::cleanup() {
  auto view = reg_.view<Health>();
  for (const auto e : view) {
    if (view.get<Health>(e).health <= 0.0f) {
      reg_.destroy(e);
      add_box();
    }
  }
}

void GameScreen::movement(float t) {
  auto view = reg_.view<Position, Velocity, Angle>();
  for (const auto e : view) {
    pos& p = view.get<Position>(e).p;
    float& vel = view.get<Velocity>(e).vel;
    float& angle = view.get<Angle>(e).angle;

    if (reg_.all_of<Rotation>(e)) {
      const float rot = reg_.get<Rotation>(e).rot;
      angle += rot * t;
    }

    if (reg_.all_of<TargetDir>(e)) {
      const float target = reg_.get<TargetDir>(e).target;
      angle += std::clamp(target - angle, -t, t);
    }

    if (reg_.all_of<Accelleration>(e)) {
      const float accel = reg_.get<Accelleration>(e).accel;
      vel = (vel + accel * t) * 0.99;
    }

    if (reg_.all_of<MaxVelocity>(e)) {
      const float max = reg_.get<MaxVelocity>(e).max;
      if (vel > max) vel = max;
    }

    p += pos::polar(vel, angle);

    if (reg_.all_of<ScreenWrap>(e)) {
      while (p.x < 0) p.x += kConfig.graphics.width;
      while (p.x > kConfig.graphics.width) p.x -= kConfig.graphics.width;
      while (p.y < 0) p.y += kConfig.graphics.height;
      while (p.y > kConfig.graphics.height) p.y -= kConfig.graphics.height;
    }

    if (reg_.all_of<Bullet>(e)) {
      if (p.x < 0 || p.x > kConfig.graphics.width) reg_.destroy(e);
      if (p.y < 0 || p.y > kConfig.graphics.height) reg_.destroy(e);
    }
  }
}

void GameScreen::fading(float t) {
  auto view = reg_.view<Fade>();
  for (const auto e : view) {
    Fade& fade = view.get<Fade>(e);
    fade.elapsed += t;
    if (fade.elapsed > fade.time && fade.dir == Fade::out) reg_.destroy(e);
  }
}

void GameScreen::firing(float t) {
  auto view = reg_.view<Firing, Position, Angle>();
  for (const auto e : view) {
    Firing& gun = view.get<Firing>(e);

    gun.time += t;
    if (gun.time > gun.rate) {
      gun.time -= gun.rate;
      const pos p = view.get<Position>(e).p;
      const float a = view.get<Angle>(e).angle;

      const auto bullet = reg_.create();
      reg_.emplace<Bullet>(bullet, e);
      reg_.emplace<Position>(bullet, pos{p.x + 5 * std::cos(a), p.y + 5 * std::sin(a)});
      reg_.emplace<Velocity>(bullet, 13);
      reg_.emplace<Angle>(bullet, a);
    }
  }
}

void GameScreen::flocking() {
  auto view = reg_.view<Flocking, Position, Velocity, Angle>();
  for (const auto e : view) {
    const pos boid = view.get<Position>(e).p;
    const float angle = view.get<Angle>(e).angle;
    float& vel = view.get<Velocity>(e).vel;

    int count = 0;
    pos center, flock, avoid;
    pos v = pos::polar(vel, angle);

    auto nearby = reg_.view<Flocking, Position, Velocity, Angle>();
    for (const auto other : nearby) {
      if (other == e) continue;

      pos p = nearby.get<Position>(other).p;
      float d = p.dist2(boid);

      // close enough to see
      if (d < 75.0f * 75.0f) {
        ++count;
        center += p;
        flock += pos::polar(nearby.get<Velocity>(other).vel, nearby.get<Angle>(other).angle);
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

  auto view = reg_.view<StayInBounds, Position, Velocity, Angle>();
  for (const auto e : view) {
    const pos p = view.get<Position>(e).p;
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

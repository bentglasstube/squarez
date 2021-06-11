#pragma once

struct polar;

struct pos {
  float x = 0, y = 0;

  constexpr pos operator+(const pos other) const { return {x + other.x, y + other.y}; }
  constexpr pos operator-(const pos other) const { return {x - other.x, y - other.y}; }
  constexpr pos operator*(const float n) const { return { x * n, y * n }; }
  constexpr pos operator/(const float n) const { return { x / n, y / n }; }

  constexpr pos& operator+=(const pos other) { x += other.x; y += other.y; return *this; }
  constexpr pos& operator-=(const pos other) { x -= other.x; y -= other.y; return *this; }
  constexpr pos& operator*=(const float n) { x *= n; y *= n; return *this; }
  constexpr pos& operator/=(const float n) { x /= n; y /= n; return *this; }

  constexpr bool operator==(const pos other) const { return x == other.x && y == other.y; }
  constexpr bool operator!=(const pos other) const { return x != other.x || y != other.y; }

  constexpr float dist2(const pos other) const {
    const float dx = x - other.x;
    const float dy = y - other.y;
    return dx * dx + dy * dy;
  }

  constexpr float angle() const { return std::atan2(y, x); }
  constexpr float mag() const { return std::sqrt(x * x + y * y); }

  constexpr static pos polar(float r, float theta) { return { r * std::cos(theta), r * std::sin(theta) }; }
};


struct rect {
  float left = 0, top = 0, right = 0, bottom = 0;

  constexpr float x() const { return left; }
  constexpr float y() const { return top; }
  constexpr float width() const { return right - left; }
  constexpr float height() const { return bottom - top; }
  constexpr float area() const { return width() * height(); }

  constexpr bool intersect(const rect other) const {
    return left < other.right && right > other.left && top < other.bottom && bottom > other.top;
  };

  constexpr bool contains(const pos p) const {
    return left < p.x && right > p.x && top < p.y && bottom > p.y;
  };
};

namespace {
  constexpr uint32_t make_color(float r, float g, float b) {
    return
      (uint32_t)(std::round(255 * r)) << 24 |
      (uint32_t)(std::round(255 * g)) << 16 |
      (uint32_t)(std::round(255 * b)) << 8 |
      0xff;
  }
}

struct hsl {
  float hue = 0, sat = 0, light = 0;

  constexpr operator uint32_t() const {
    assert(hue >= 0 && hue < 360);
    assert(sat >= 0.0f && sat <= 1.0f);
    assert(light >= 0.0f && light <= 1.0f);

    const float c = (1 - std::abs(2 * light - 1)) * sat;
    const float x = c * (1 - std::abs(std::fmod(hue / 60, 2) - 1));
    const float m = light - c / 2.0f;

    if (hue < 60)  return make_color(c + m, x + m, m);
    if (hue < 120) return make_color(x + m, c + m, m);
    if (hue < 180) return make_color(m, c + m, x + m);
    if (hue < 240) return make_color(m, x + m, c + m);
    if (hue < 300) return make_color(x + m, m, c + m);
    else           return make_color(c + m, m, x + m);
  };
};

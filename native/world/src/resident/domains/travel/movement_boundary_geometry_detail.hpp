#pragma once
#include "movement_exact_detail.hpp"
#include <array>
namespace hanmo::resident::movement_boundary_detail {
using namespace movement_detail;
// Ephemeral signed view over the one existing CanonicalFraction arithmetic.
// No new persisted numeric representation or approximate contact time.
struct Signed {
  bool negative = false;
  CanonicalFraction magnitude;
};
struct Point {
  Signed x, y;
};
inline Signed normalized(Signed v, ExactArithmeticBudget b) {
  if (compare(v.magnitude, {}, b) == 0)
    v.negative = false;
  return v;
}
inline Signed value(Integer v, ExactArithmeticBudget b) {
  char digits[32];
  const auto text = std::to_chars(digits, digits + sizeof(digits), v);
  if (text.ec != std::errc{})
    fail("BoundaryCoordinateOverflow");
  const bool negative = v < 0;
  const char *first = digits + (negative ? 1 : 0);
  return {negative, take(normalize_fraction(
                        std::string_view(first, text.ptr - first), "1", b))};
}
inline Signed value(double v, ExactArithmeticBudget b) {
  return normalized({v < 0, take(fraction_from_binary64(v < 0 ? -v : v, b))},
                    b);
}
inline Signed negate(Signed v, ExactArithmeticBudget b) {
  v.negative = !v.negative;
  return normalized(std::move(v), b);
}
inline Signed plus(const Signed &a, const Signed &c, ExactArithmeticBudget b) {
  if (a.negative == c.negative)
    return normalized({a.negative, add(a.magnitude, c.magnitude, b)}, b);
  const auto order = compare(a.magnitude, c.magnitude, b);
  return order >= 0
             ? normalized({a.negative, sub(a.magnitude, c.magnitude, b)}, b)
             : normalized({c.negative, sub(c.magnitude, a.magnitude, b)}, b);
}
inline Signed minus(const Signed &a, const Signed &c, ExactArithmeticBudget b) {
  return plus(a, negate(c, b), b);
}
inline Signed times(const Signed &a, const Signed &c, ExactArithmeticBudget b) {
  return normalized(
      {a.negative != c.negative, mul(a.magnitude, c.magnitude, b)}, b);
}
inline Signed divided(const Signed &a, const CanonicalFraction &d,
                      ExactArithmeticBudget b) {
  return normalized({a.negative, div(a.magnitude, d, b)}, b);
}
inline int order(const Signed &a, const Signed &c, ExactArithmeticBudget b) {
  if (a.negative != c.negative)
    return a.negative ? -1 : 1;
  auto result = compare(a.magnitude, c.magnitude, b);
  return a.negative ? -result : result;
}
inline Signed interpolate(const Signed &a, const Signed &c,
                          const CanonicalFraction &u, ExactArithmeticBudget b) {
  return plus(a, times(minus(c, a, b), {false, u}, b), b);
}
inline Point interpolate(const Point &a, const Point &c,
                         const CanonicalFraction &u, ExactArithmeticBudget b) {
  return {interpolate(a.x, c.x, u, b), interpolate(a.y, c.y, u, b)};
}
// Closed rectangle clipping in the exact original local coordinate system.
// No rounded cell, projected integer location, or second moving object.
inline std::optional<CanonicalFraction>
rectangle_entry(const Point &from, const Point &to, Integer half_length_dm,
                Integer half_width_dm, ExactArithmeticBudget b) {
  if (half_length_dm <= 0 || half_width_dm <= 0)
    fail("BoundaryRectangleRequired");
  const auto lx = value(half_length_dm, b), ly = value(half_width_dm, b);
  const std::array<Signed, 4> a{plus(from.x, lx, b), minus(lx, from.x, b),
                                plus(from.y, ly, b), minus(ly, from.y, b)};
  const std::array<Signed, 4> z{plus(to.x, lx, b), minus(lx, to.x, b),
                                plus(to.y, ly, b), minus(ly, to.y, b)};
  CanonicalFraction enter, leave = integer(1, b);
  bool outside = false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].negative)
      outside = true;
    if (a[i].negative && z[i].negative)
      return {};
    if (a[i].negative == z[i].negative)
      continue;
    const auto u =
        div(a[i].magnitude, add(a[i].magnitude, z[i].magnitude, b), b);
    if (a[i].negative) {
      if (compare(u, enter, b) > 0)
        enter = u;
    } else if (compare(u, leave, b) < 0)
      leave = u;
  }
  if (compare(enter, leave, b) > 0)
    return {};
  // A prefix already strictly inside this field is not a new boundary entry.
  if (!outside) {
    bool on_edge = false;
    for (const auto &q : a)
      if (compare(q.magnitude, {}, b) == 0)
        on_edge = true;
    if (!on_edge)
      return {};
  }
  return enter;
}
} // namespace hanmo::resident::movement_boundary_detail

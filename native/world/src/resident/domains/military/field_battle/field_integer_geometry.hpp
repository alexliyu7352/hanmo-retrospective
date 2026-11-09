// 连续战场的整数距离与朝向；运行规则不调用浮点三角函数。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <array>
#include <limits>
namespace hanmo::resident {
// 128 位平方的整数开方下界；坐标量化与旧路线弧长使用同一精度。
inline Integer field_integer_sqrt(__uint128_t value) {
  __uint128_t low = 0, high = std::numeric_limits<Integer>::max();
  while (low < high) {
    const auto mid = (low + high + 1) / 2;
    if (mid * mid <= value)
      low = mid;
    else
      high = mid - 1;
  }
  return static_cast<Integer>(low);
}
// 保留平方精度供半径/碰撞比较；不提前截断为米。
inline __int128_t field_distance_squared(PointI a, PointI b) {
  const __int128_t x = __int128_t(a.x) - b.x, y = __int128_t(a.y) - b.y;
  return x * x + y * y;
}
// CORDIC 常数以十亿分之一度表示 atan(2^-i)，最终才量化为千分度。
inline Integer field_direction_mdeg(PointI delta) {
  if (delta.x == 0 && delta.y == 0)
    return 0;
  static constexpr std::array<Integer, 32> angles = {
      45000000000, 26565051177, 14036243468, 7125016349, 3576334375, 1789910608,
      895173710,   447614171,   223810500,   111905677,  55952892,   27976453,
      13988227,    6994114,     3497057,     1748528,    874264,     437132,
      218566,      109283,      54642,       27321,      13660,      6830,
      3415,        1708,        854,         427,        213,        107,
      53,          27};
  __int128_t x = __int128_t(delta.x) * (Integer(1) << 30);
  __int128_t y = __int128_t(delta.y) * (Integer(1) << 30);
  Integer angle = 0;
  if (x < 0) {
    x = -x;
    y = -y;
    angle = 180000000000;
  }
  for (std::size_t i = 0; i < angles.size() && y != 0; ++i) {
    const auto old_x = x;
    if (y > 0) {
      x += y >> i;
      y -= old_x >> i;
      angle += angles[i];
    } else {
      x -= y >> i;
      y += old_x >> i;
      angle -= angles[i];
    }
  }
  angle %= 360000000000;
  if (angle < 0)
    angle += 360000000000;
  return angle / 1000000;
}
} // namespace hanmo::resident

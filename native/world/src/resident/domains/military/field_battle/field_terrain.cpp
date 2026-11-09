// 细地形格式、内容摘要与纯整数采样；不依赖随机源或世界可变状态。
#include "field_terrain.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <numeric>
#include <picosha2.h>
namespace hanmo::resident {
namespace {
using Wide = __int128_t;
using UWide = __uint128_t;
// 拒绝保持合同错误码，路径用于指出坏的作者输入。
template <class T> Outcome<T> missing(const StableId &path) {
  Outcome<T> result;
  result.error.original = {"FieldTerrainMissing", path};
  return result;
}
// JSON 浮点仅在装载边界转最短十进制；之后保留精确整数比例。
bool scale(const BoundaryValue *value, FieldScale &out) {
  if (!value)
    return false;
  if (auto i = value->as<Integer>()) {
    out = {*i, 1};
    return *i > 0;
  }
  auto d = value->as<double>();
  if (!d)
    return false;
  char buffer[64];
  auto converted = std::to_chars(buffer, buffer + sizeof buffer, *d);
  if (converted.ec != std::errc{})
    return false;
  std::string_view text(buffer, converted.ptr - buffer);
  Integer n = 0, den = 1;
  bool fraction = false;
  std::size_t at = 0;
  for (; at < text.size() && text[at] != 'e' && text[at] != 'E'; ++at) {
    char c = text[at];
    if (c == '.' && !fraction) {
      fraction = true;
      continue;
    }
    if (c < '0' || c > '9' ||
        n > (std::numeric_limits<Integer>::max() - (c - '0')) / 10)
      return false;
    n = n * 10 + c - '0';
    if (fraction) {
      if (den > 100000000)
        return false;
      den *= 10;
    }
  }
  if (at < text.size()) {
    ++at;
    bool negative = false;
    if (at < text.size() && (text[at] == '+' || text[at] == '-'))
      negative = text[at++] == '-';
    int exponent = 0;
    auto parsed =
        std::from_chars(text.data() + at, text.data() + text.size(), exponent);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        exponent > 9)
      return false;
    for (int i = 0; i < exponent; ++i) {
      auto &target = negative ? den : n;
      if (target > std::numeric_limits<Integer>::max() / 10)
        return false;
      target *= 10;
    }
  }
  if (n <= 0)
    return false;
  auto divisor = std::gcd(n, den);
  out = {n / divisor, den / divisor};
  return true;
}
// 必须是既有 JSON 字符串；不接受隐式数字或空 ID。
const std::string *string(const BoundaryValue *value) {
  return value ? value->as<std::string>() : nullptr;
}
// 坐标与中间权重使用 128 位，截断仅在明确的定点边界发生。
Integer clamp_wide(Wide value, Integer high) {
  return static_cast<Integer>(std::clamp<Wide>(value, 0, high));
}
// 五次幂只处理 Q20 的 [0,1]，乘以 255 仍在无符号 128 位范围内。
UWide fifth(Integer value) {
  UWide v = static_cast<UWide>(value);
  return v * v * v * v * v;
}
// level^2.2 = level^2 * fifth_root(level)，Q20 二分；毫米结果始终确定。
Integer half_width(std::uint8_t level) {
  constexpr Integer q = 1 << 20;
  Integer lo = 0, hi = q;
  const UWide rhs = static_cast<UWide>(level) * fifth(q);
  while (lo < hi) {
    Integer mid = (lo + hi + 1) / 2;
    if (fifth(mid) * 255 <= rhs)
      lo = mid;
    else
      hi = mid - 1;
  }
  return 38400 + static_cast<Integer>(static_cast<Wide>(281600) * level *
                                      level * lo / (65025 * q));
}
} // namespace
Outcome<FieldTerrainMetadata>
decode_field_terrain_metadata(const BoundaryValue &value) {
  FieldTerrainMetadata result;
  auto format = string(value.find("format")),
       map = string(value.find("map_id"));
  auto w = value.find("width_px"), h = value.find("height_px");
  if (!format || *format != "hanmo-world-terrain/1" || !map || map->empty() ||
      !w || !h || !w->as<Integer>() || !h->as<Integer>())
    return missing<FieldTerrainMetadata>("terrain.json");
  result.map_id = *map;
  result.width_px = *w->as<Integer>();
  result.height_px = *h->as<Integer>();
  if (result.width_px <= 0 || result.height_px <= 0 ||
      result.width_px > std::numeric_limits<Integer>::max() / 1024 ||
      result.height_px > std::numeric_limits<Integer>::max() / 1024 ||
      static_cast<Wide>(result.width_px) * result.height_px >
          std::numeric_limits<std::size_t>::max() / 3 ||
      !scale(value.find("km_per_unit"), result.km_per_unit) ||
      !scale(value.find("m_per_px"), result.m_per_px))
    return missing<FieldTerrainMetadata>("terrain.json/scale");
  const auto *files = value.find("files");
  if (!files)
    return missing<FieldTerrainMetadata>("terrain.json/files");
  for (auto entry : {std::pair{"height", &result.height_sha256},
                     std::pair{"rivers", &result.rivers_sha256},
                     std::pair{"land", &result.land_sha256}}) {
    const auto *row = files->find(entry.first);
    const auto *path = row ? string(row->find("path")) : nullptr;
    const auto *type = row ? string(row->find("type")) : nullptr;
    const StableId expected_path =
        StableId(entry.first) == "height"   ? "height.u16"
        : StableId(entry.first) == "rivers" ? "rivers.u8x2"
                                            : "land.u8x3";
    const StableId expected_type =
        StableId(entry.first) == "height"   ? "uint16 LE"
        : StableId(entry.first) == "rivers" ? "uint8×2"
                                            : "uint8×3";
    if (!path || *path != expected_path || !type || *type != expected_type)
      return missing<FieldTerrainMetadata>(StableId("terrain.json/files/") +
                                           entry.first);
    auto digest = row ? string(row->find("sha256")) : nullptr;
    if (!digest || digest->size() != 64 ||
        !std::all_of(digest->begin(), digest->end(), [](char c) {
          return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        }))
      return missing<FieldTerrainMetadata>(StableId("terrain.json/files/") +
                                           entry.first);
    *entry.second = *digest;
  }
  return {true, std::move(result), {}};
}
Outcome<FieldTerrainData> load_field_terrain_data(
    FieldTerrainMetadata metadata, std::span<const std::uint8_t> height,
    std::span<const std::uint8_t> rivers, std::span<const std::uint8_t> land) {
  if (metadata.width_px <= 0 || metadata.height_px <= 0 ||
      metadata.m_per_px.num <= 0 || metadata.m_per_px.den <= 0)
    return missing<FieldTerrainData>("terrain.json");
  const Wide pixels = static_cast<Wide>(metadata.width_px) * metadata.height_px;
  if (pixels * 2 != height.size() || pixels * 2 != rivers.size() ||
      pixels * 3 != land.size())
    return missing<FieldTerrainData>("terrain/files/length");
  if (picosha2::hash256_hex_string(height.begin(), height.end()) !=
          metadata.height_sha256 ||
      picosha2::hash256_hex_string(rivers.begin(), rivers.end()) !=
          metadata.rivers_sha256 ||
      picosha2::hash256_hex_string(land.begin(), land.end()) !=
          metadata.land_sha256)
    return missing<FieldTerrainData>("terrain/files/sha256");
  return {true,
          {std::move(metadata),
           {height.begin(), height.end()},
           {rivers.begin(), rivers.end()},
           {land.begin(), land.end()}},
          {}};
}
FieldTerrainSample sample_field_terrain(const FieldTerrainData &data,
                                        PointI point) {
  const auto &m = data.metadata;
  const Integer fx = clamp_wide(static_cast<Wide>(point.x) * m.m_per_px.den *
                                        1024 / (Wide(m.m_per_px.num) * 10) -
                                    512,
                                (m.width_px - 1) * 1024);
  const Integer fy = clamp_wide(static_cast<Wide>(point.y) * m.m_per_px.den *
                                        1024 / (Wide(m.m_per_px.num) * 10) -
                                    512,
                                (m.height_px - 1) * 1024);
  const Integer x = fx / 1024, y = fy / 1024, wx = fx % 1024, wy = fy % 1024;
  const auto height = [&](Integer i, Integer j) -> Integer {
    const std::size_t at = 2 * (j * m.width_px + i);
    return data.height[at] | (Integer(data.height[at + 1]) << 8);
  };
  const Integer x1 = std::min(x + 1, m.width_px - 1),
                y1 = std::min(y + 1, m.height_px - 1);
  FieldTerrainSample result;
  result.height_dm =
      (height(x, y) * (1024 - wx) * (1024 - wy) +
       height(x1, y) * wx * (1024 - wy) + height(x, y1) * (1024 - wx) * wy +
       height(x1, y1) * wx * wy) /
      (1024 * 1024);
  const Integer nx = std::min((fx + 512) / 1024, m.width_px - 1),
                ny = std::min((fy + 512) / 1024, m.height_px - 1);
  const std::size_t at = ny * m.width_px + nx;
  const auto distance = data.rivers[2 * at];
  result.river_level = data.rivers[2 * at + 1];
  result.river_distance_mm = 4000000LL * distance * distance / 65025;
  result.river_half_width_mm = half_width(result.river_level);
  result.water = result.river_distance_mm < result.river_half_width_mm;
  result.cultivation = data.land[3 * at];
  result.forest = data.land[3 * at + 1];
  result.rock = data.land[3 * at + 2];
  return result;
}
PointI field_map_point_dm(const FieldTerrainMetadata &m, PointI point) {
  return {static_cast<Integer>(static_cast<Wide>(point.x) * m.km_per_unit.num *
                               10000 / m.km_per_unit.den),
          static_cast<Integer>(static_cast<Wide>(point.y) * m.km_per_unit.num *
                               10000 / m.km_per_unit.den)};
}
} // namespace hanmo::resident

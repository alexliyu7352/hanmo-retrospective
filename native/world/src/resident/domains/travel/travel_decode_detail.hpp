// 原行旅 catalog/seed 共享的私有严格读取叶；函数体与首错身份保持原样。
#pragma once
#include "resident/checkpoint/ordered_value.hpp"
#include "travel.hpp"
namespace hanmo::resident::travel_decode_detail {
using Value = BoundaryValue;
using Dict = Value::Dict;
using Array = Value::Array;
// 边界失败只携带规则码与字段路径，外层不得发布部分解码。
struct DecodeFailure {
  RuleError error;
};
// 统一拒绝错误类型，不允许字符串、布尔或浮点冒充整数。
[[noreturn]] inline void bad(const StableId &code, const StableId &path) {
  throw DecodeFailure{{{code, path},
                       code.rfind("UnsupportedNative", 0) == 0
                           ? ErrorKind::unsupported_capability
                           : ErrorKind::rule,
                       {},
                       "travel",
                       {},
                       {}}};
}
// 必填键只读查询，无缺省字段自动写入。
[[gnu::no_dangling]] inline const Value &
field(const Value &v, const StableId &key, const StableId &path) {
  const auto *x = v.find(key);
  if (!x)
    bad("InvalidTerrainJourney", path + "/" + key);
  return *x;
}
// typed边界取值，标签不符保首错。
template <class T>
[[gnu::no_dangling]] const T &typed(const Value &v, const StableId &path) {
  const auto *x = v.as<T>();
  if (!x)
    bad("InvalidTerrainJourney", path);
  return *x;
}
// 字段整数不经double，保留int64全域。
inline Integer integer(const Value &v, const StableId &key,
                       const StableId &path) {
  return typed<Integer>(field(v, key, path), path + "/" + key);
}
// 字段字符串不隐式转换ID。
inline StableId text(const Value &v, const StableId &key,
                     const StableId &path) {
  return typed<StableId>(field(v, key, path), path + "/" + key);
}
// 字段布尔不以真假值宽松转换。
inline bool boolean(const Value &v, const StableId &key, const StableId &path) {
  return typed<bool>(field(v, key, path), path + "/" + key);
}
// 固定二维整数点，浮点数组不接受为权威坐标。
inline PointI point(const Value &v, const StableId &path) {
  const auto &a = typed<Array>(v, path);
  if (a.size() != 2)
    bad("InvalidTerrainJourney", path);
  return {typed<Integer>(a[0], path + "/0"), typed<Integer>(a[1], path + "/1")};
}
// 路线点逐项保序，重复点由领域规则判断。
inline std::vector<PointI> points(const Value &v, const StableId &path) {
  std::vector<PointI> result;
  for (const auto &x : typed<Array>(v, path))
    result.push_back(point(x, path));
  return result;
}
// 字符串数组不排序，不删除重复的已会面行动。
inline std::vector<StableId> strings(const Value &v, const StableId &path) {
  std::vector<StableId> result;
  for (const auto &x : typed<Array>(v, path))
    result.push_back(typed<StableId>(x, path));
  return result;
}
// 二选一目的地保标签，城市ID不转换坐标。
inline TravelDestination destination(const Value &v, const StableId &path) {
  if (auto *id = v.as<StableId>())
    return *id;
  return point(v, path);
}
// 所有动态字段必须被owner明确识别，未知字段不能进入兼容extras。
inline void fields(const Value &v, std::initializer_list<const char *> allowed,
                   const StableId &path) {
  for (const auto &row : typed<Dict>(v, path)) {
    bool found = false;
    for (const auto *key : allowed)
      if (row.first == key) {
        found = true;
        break;
      }
    if (!found)
      bad("UnsupportedNativeTravelField", path + "/" + row.first);
  }
}
// 模式解析不允许未知值降级为foot。
inline terrain::Mode mode(const StableId &value, const StableId &path) {
  if (value == "foot")
    return terrain::Mode::foot;
  if (value == "army")
    return terrain::Mode::army;
  if (value == "boat")
    return terrain::Mode::boat;
  bad("InvalidTravelMode", path);
}
} // namespace hanmo::resident::travel_decode_detail

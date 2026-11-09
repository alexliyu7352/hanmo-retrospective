// 地形几何只读值与纯查询；权威坐标保持int64，临时积分保持binary64。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "hanmo/resident/model_contract.hpp"
#include "hanmo/resident/world_moment.hpp"
#include <map>
#include <optional>
#include <unordered_map>

namespace hanmo::resident::terrain {
using ::hanmo::resident::PointI;
inline constexpr Integer MAX_COST = 2147483647;
inline constexpr double EPSILON = 0.000000001;

// 模式只能显式选择；越界枚举仍在公开查询入口被拒绝。
enum class Mode { foot, army, boat };
// 单格保留可选字段的存在性；空crossing/blocked_flag不得冒充缺省值。
struct Cell {
  std::string terrain = "plain";
  std::optional<std::string> crossing, blocked_flag;
};
// 作者覆盖只存真整数；未知成本键由validate_map按旧错误码拒绝。
using ModeCosts = CheckpointMap<std::string, Integer>;
using Costs = CheckpointMap<Mode, ModeCosts>;
using Budgets = CheckpointMap<Mode, Integer>;
using Flags = CheckpointHashMap<std::string, bool>;
// 目录保留行旅使用的计价与预算字段；几何本身不读取或改写它们。
struct TerrainMap {
  StableId id;
  Integer width = 0, height = 0, cell_size = 0;
  std::vector<Cell> cells;
  Costs costs;
  Budgets budgets;
  Integer money_per_tick = 0, fatigue_per_tick = 0;
};
// 小数格界交点只在一次轨迹计算内存在，不作为人物权威位置。
struct PointD {
  double x = 0.0, y = 0.0;
};
// 相邻同费率区间合并，累计成本保留原有积分顺序与末段截齐行为。
struct SegmentSpan {
  PointD from, to;
  double start_cost = 0.0, end_cost = 0.0;
  Integer rate = 0;
};
struct SegmentCost {
  Integer cost = 0;
};
struct SegmentProfile {
  Integer cost = 0;
  double raw_cost = 0.0;
  std::vector<SegmentSpan> spans;
};
// 折线只包含整数起终点和格心，逐段向上取整后才累加总成本。
struct Route {
  std::vector<PointI> points;
  std::vector<Integer> segment_costs;
  Integer cost = 0;
};
// 连续起点与后续整数路点分开返回；纯几何临时结果不新增持久位置或世界状态。
struct ContinuousRoute {
  PointD origin;
  std::vector<PointI> points;
  std::vector<Integer> segment_costs;
  Integer cost = 0;
};

// 检查尺寸、逐行格子、关口及覆盖成本；不承担目录外层字段类型解码。
Status validate_map(const TerrainMap &map);
// 公共驻留查询遇到任何地图、模式、坐标或阻断错误均返回false。
bool point_valid(const TerrainMap &map, PointI point, Mode mode = Mode::foot,
                 const Flags &flags = {});
// 同一格界切分积分只返回总成本；失败保旧code，不暴露部分积分。
Outcome<SegmentCost> segment_cost(const TerrainMap &map, PointI from, PointI to,
                                  Mode mode = Mode::foot,
                                  const Flags &flags = {});
// 返回同源积分的完整成本变化轨迹；flags仅借用且永不改写。
Outcome<SegmentProfile> segment_profile(const TerrainMap &map, PointI from,
                                        PointI to, Mode mode = Mode::foot,
                                        const Flags &flags = {});
// 原A*按estimate、cost、node稳定决胜；不预演时钟、人物移动或接触。
Outcome<Route> plan(const TerrainMap &map, PointI from, PointI to,
                    Mode mode = Mode::foot, const Flags &flags = {});

// prepared入口仅供已成功validate_map且随后未改变的只读目录；调用者保证借用有效。
// 每次仍拒绝非法Mode，point_valid返回false，其余保留InvalidTravelMode；不重扫cells。
bool point_valid_prepared(const TerrainMap &map, PointI point,
                          Mode mode = Mode::foot, const Flags &flags = {});
Outcome<SegmentCost> segment_cost_prepared(const TerrainMap &map, PointI from,
                                           PointI to, Mode mode = Mode::foot,
                                           const Flags &flags = {});
Outcome<SegmentProfile> segment_profile_prepared(const TerrainMap &map,
                                                 PointI from, PointI to,
                                                 Mode mode = Mode::foot,
                                                 const Flags &flags = {});
Outcome<Route> plan_prepared(const TerrainMap &map, PointI from, PointI to,
                             Mode mode = Mode::foot, const Flags &flags = {});
// 连续prepared入口沿相同已验地图约定；非法模式、坐标与阻断保留原拒绝。
bool point_valid_continuous_prepared(const TerrainMap &map, PointD point,
                                     Mode mode, const Flags &flags);
Outcome<SegmentProfile>
segment_profile_continuous_prepared(const TerrainMap &map, PointD from,
                                    PointD to, Mode mode, const Flags &flags);
// 只把实际起点改为连续坐标，后续整数路点和成本仍由已有A*实现产生。
Outcome<ContinuousRoute> plan_continuous_prepared(const TerrainMap &map,
                                                  PointD from, PointI to,
                                                  Mode mode, const Flags &flags,
                                                  ExactArithmeticBudget budget);
// 有界精确预算驱动原连续二分；失败原样返回，不赠送预算或吸附到整数点。
Outcome<PointD>
partial_point_continuous_prepared(const TerrainMap &map, PointD from, PointI to,
                                  Mode mode, const Flags &flags,
                                  const CanonicalFraction &budget,
                                  ExactArithmeticBudget arithmetic);
} // namespace hanmo::resident::terrain

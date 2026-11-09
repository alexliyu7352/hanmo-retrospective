// 同一作者地形、路线和版本确定战场；生成结果不是可编辑的第二套地图。
#pragma once
#include "field_terrain.hpp"
namespace hanmo::resident {
// 数值来自合同默认作者配置；一场会战固定持有其版本和完整参数。
struct FieldBattleParameters {
  StableId version = "field_battle/1";
  Integer window_s = 21600, step_s = 10;
  Integer length_m = 4000, width_m = 3000, cell_m = 25;
  Integer deployment_length_m = 600, deployment_width_m = 1600,
          deployment_inset_m = 300;
  Integer deployment_min_percent = 40, exit_radius_m = 800;
  Integer anchor_shift_step_m = 500, anchor_shift_limit_m = 4000;
  Integer courier_m_per_minute = 500, minimum_order_delay_s = 60;
  Integer pulse_s = 60, round_equiv_s = 3600;
  Integer morale_loss_bp = 20000, commander_exposure_bp = 3000;
  Integer max_segments = 4;
  Integer infantry_m_per_minute = 50, archer_m_per_minute = 50;
  Integer cavalry_m_per_minute = 150, charge_m_per_minute = 300;
  Integer rout_speed_bp = 16000;
  // 行按下方 FieldTerrainKind
  // 的稳定枚举，列依次为步、弓、骑；零只表示不可通行。
  std::array<std::array<Integer, 3>, 13> terrain_speed_bp{
      {{{10000, 10000, 10000}},
       {{10000, 10000, 10000}},
       {{6000, 6000, 4000}},
       {{6000, 6000, 5000}},
       {{0, 0, 0}},
       {{4000, 4000, 4000}},
       {{0, 0, 0}},
       {{10000, 10000, 10000}},
       {{10000, 10000, 10000}},
       {{4000, 4000, 4000}},
       {{6000, 6000, 5000}},
       {{0, 0, 0}},
       {{0, 0, 0}}}};
  std::array<Integer, 13> terrain_cover{
      {0, 0, 20, 10, 0, 0, 0, 0, 0, 0, 25, 0, 0}};
  Integer melee_contact_m = 5, archer_min_range_m = 30,
          archer_max_range_m = 180;
  Integer ai_retreat_fit_percent = 40, ai_retreat_morale = 30;
  Integer ai_support_morale = 50, ai_eval_s = 600, ai_advance_hold_s = 600;
  Integer rejoin_distance_m = 200, rejoin_stopped_s = 300;
  Integer max_formations_per_side = 32;
};
struct FieldAnchor {
  StableId route_id;
  PointI point_m, tangent;
};
enum class FieldTerrainKind {
  Plain,
  Field,
  Forest,
  Rough,
  Steep,
  Stream,
  Water,
  Road,
  Bridge,
  Ford,
  Village,
  Wall,
  Gate
};
static_assert(static_cast<std::size_t>(FieldTerrainKind::Gate) + 1 == 13);
// 同一完整参数门供作者准入和生成器调用；不靠某一入口的默认值补缺。
Status validate_field_battle_parameters(const FieldBattleParameters &);
struct FieldCell {
  Integer height_dm = 0;
  FieldTerrainKind terrain = FieldTerrainKind::Plain;
  bool blocks_sight = false;
  Integer cover = 0;
  bool operator==(const FieldCell &) const = default;
};
// 城、村种类来自原 settlement.kind，位置由原 TravelCatalog 和同一地图尺度得出。
struct FieldSettlement {
  StableId id;
  PointI point_dm;
  bool city = false;
};
struct FieldRoadGeometry {
  StableId route_id;
  std::vector<PointI> points_dm;
  std::vector<FieldSettlement> settlements;
};
struct FieldSideZone {
  PointI road_exit_dm, deployment_center_dm;
  PointI deployment_axis_dm; // actual local original rectangle along-road axis
  std::vector<std::size_t> deploy_cells, exit_cells;
  Integer rectangle_cells = 0;
  // Pure original road/settlement geometry, not permission to enter it.
  // P3 must still qualify this actual settlement before its cell is an exit.
  std::optional<StableId> road_exit_settlement_id;
  // Derived actual route-crossed Gate or authored village-centre cell.
  // Missing on a selected settlement means original geometry is unavailable.
  std::optional<std::size_t> settlement_exit_cell;
};
struct GeneratedBattleField {
  StableId content_version, field_hash;
  FieldBattleParameters parameters;
  FieldAnchor anchor;
  Integer route_distance_dm = 0, columns = 0, rows = 0;
  std::vector<FieldCell> cells;
  std::array<FieldSideZone, 2> sides;
};
// 只接受真实路线和非空已验地形；根据 §2.4 原次序选锚，不另造平地。
Outcome<GeneratedBattleField>
generate_battle_field(const FieldTerrainData &, const FieldRoadGeometry &,
                      Integer contact_distance_dm,
                      const FieldBattleParameters &,
                      const StableId &content_version);
// 仅在实际城村出口经外交过滤后仍为空时调用；不判断敌我，不使 Gate 可穿行。
// 原有场界候选同样使用本函数，等距按实际格序，不另造路线或地形。
Outcome<std::vector<std::size_t>>
read_field_boundary_exit_cells(const GeneratedBattleField &,
                               PointI road_exit_dm, bool nearest_fallback);
// 原作者折线的整数弧长；接触投影与生成器使用同一分段长度，不再重算另一条线。
Integer field_road_length_dm(const FieldRoadGeometry &);
// 规则与读视图共用坐标变换和通行判定，不按 UI 像素另算。
PointI field_cell_center_dm(const GeneratedBattleField &, std::size_t index);
PointI field_point_to_map_dm(const FieldAnchor &, PointI local_dm);
bool field_cell_passable(FieldTerrainKind);
} // namespace hanmo::resident

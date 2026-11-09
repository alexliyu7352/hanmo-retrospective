// 具名看管历史归原 Military 根；期限不是第二份在押名单，转手不重起期限。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "resident/kernel/source_ref.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "resident/domains/travel/movement_exact_model.hpp"
namespace hanmo::resident {
struct NamedMilitaryCustodyPolicy {
  StableId rule_id;
  Integer duration_half_days = 0;
};
struct NamedMilitaryCustodyTerm {
  StableId id, actor_id, captured_army_id, captured_organization_id;
  SourceRef capture_source, capture_application_source, army_formation_source, policy_source;
  Integer capture_actor_ordinal = -1, capture_application_schema = 1;
  Integer army_formation_schema = 1;
  Integer capture_tick = 0, recorded_tick = 0;
  Integer duration_half_days = 0, due_tick = 0;
  // adopted_at 反读原捕获应用 M，due_at 用原精确半日加法；粗 Tick 不驱动期满。
  WorldMoment adopted_at, due_at;
  FieldOrder field_order;
};
// 关闭只记录真正执行时刻及原位置/道路/控制选择证据；当前占用仍只读 prisoner_ids。
struct NamedMilitaryCustodyRelease {
  StableId id, term_id, actor_id, keeper_army_id, keeper_organization_id;
  SourceRef prior_custody_source;
  WorldMoment released_at;
  Integer recorded_tick = 0;
  MovementAnchor origin_anchor;
  std::vector<MomentOwnerSource> origin_sources;
  // 0 为完整两级查询后的原地，1 为原被俘军团势力，2 为看管方以外。
  Integer destination_priority = 0;
  StableId destination_settlement_id, jurisdiction_id;
  CanonicalFraction road_distance;
  std::vector<StableId> road_ids;
  bool initial_returning = false;
  MovementAnchor released_anchor;
  FieldOrder field_order;
};
// 关闭必须另有完整执行来源，不能靠一个布尔值或缺失看管人解除门禁。
struct NamedMilitaryCustodyHistory {
  std::vector<NamedMilitaryCustodyTerm> terms;
  std::vector<NamedMilitaryCustodyRelease> closures;
  FieldOrder field_order;
};
} // namespace hanmo::resident

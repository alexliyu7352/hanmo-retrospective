#pragma once
#include "political_relations.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/domains/military/field_battle/field_started_source.hpp"
namespace hanmo::resident {
struct PoliticalRelationsReadContext {
  const CommonStore &common;
  const OrganizationState &organizations;
  const OrganizationCatalog &organization_catalog;
  const PoliticalRelationsState &state;
  const PoliticalRelationsCatalog &catalog;
  const MilitaryState &military;
  const FieldBattleWorldState &field;
  const FieldSegmentApplicationRead &actual_clock;
  const MilitaryFormationSourceRead &formations;
};
struct PoliticalRelationView {
  PoliticalRelation relation = PoliticalRelation::neutral;
  SourceRef initialization;
  std::optional<SourceRef> last_change;
};
// 同一次完整原读借真实Common头；任何世界写入后必须重取，不持有临时SourceRef。
struct PoliticalRelationOriginalView {
  PoliticalRelation relation = PoliticalRelation::neutral;
  const FactRecord *initialization = nullptr;
  const FactRecord *last_change = nullptr;
};
Outcome<PoliticalRelationOriginalView>
read_current_political_relation_original_view(
    const PoliticalRelationsReadContext &, const StableId &, const StableId &);
Outcome<PoliticalRelationOriginalView>
read_political_relation_original_view_before_field_source(
    const PoliticalRelationsReadContext &, const SourceRef &, const StableId &,
    const StableId &);
Status validate_political_relations(const PoliticalRelationsReadContext &);
// 仅 P3 三种原事实：完整同源验证后借真实 Common 头与唯一原时刻。
// 背盟的时刻借已验证父 change，不存第二份时间；写入原根后必须重取。
struct PoliticalRelationsOriginalSource {
  const FactRecord *actual_source = nullptr;
  const WorldMoment *occurred_at = nullptr;
  Integer source_schema = 1;
};
Outcome<PoliticalRelationsOriginalSource>
read_political_relations_original_source(const PoliticalRelationsReadContext &,
                                         const MomentOwnerSource &);
Outcome<PoliticalRelationView>
read_current_political_relation_view(const PoliticalRelationsReadContext &,
                                     const StableId &, const StableId &);
Outcome<PoliticalRelationView> read_political_relation_view_before_field_source(
    const PoliticalRelationsReadContext &, const SourceRef &, const StableId &,
    const StableId &);
Outcome<PoliticalRelation>
read_current_political_relation(const PoliticalRelationsReadContext &,
                                const StableId &, const StableId &);
// Only actual original Field contact/start sources; not an arbitrary queried M.
Outcome<PoliticalRelation> read_political_relation_before_field_source(
    const PoliticalRelationsReadContext &, const SourceRef &, const StableId &,
    const StableId &);
// root_memory 必须为同一候选 World 首部持久 checkpoint_copy_memory；
// 不接受临时租约替代。失败后丢弃整个候选；原 World owner 验实际根/同 job。
Status initialize_political_relations_owned_in_candidate(
    CommonStore &, PoliticalRelationsState &,
    const PoliticalRelationsReadContext &, CheckpointMemoryCharge &root_memory);
// 旧 ABI 仅兼容，不提供初始化根字段的持久拥有保证。
Status initialize_political_relations_in_candidate(
    CommonStore &, PoliticalRelationsState &,
    const PoliticalRelationsReadContext &);
// 正式同候选采用只返回唯一Common原头；原行复制在插表前真实预租。
// 任意后续Common表写入前消费/复制，之后需按原ID重借，不保留旧头指针。
// 初始化正式调用仍须中央实际 World 首部租约接线，不据单个出口称全 P3 就绪。
Outcome<const FactRecord *> adopt_field_battle_relation_fact_in_candidate(
    CommonStore &, PoliticalRelationsState &,
    const PoliticalRelationsReadContext &,
    const MomentOwnerSource &actual_start);
Outcome<SourceRef>
adopt_field_battle_relation_in_candidate(CommonStore &,
                                         PoliticalRelationsState &,
                                         const PoliticalRelationsReadContext &,
                                         const MomentOwnerSource &actual_start);
} // namespace hanmo::resident

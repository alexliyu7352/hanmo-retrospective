// 真实读供货告示的唯一来源；原报告和观察时刻仍由经济与公共知识原 owner 保有。
#pragma once
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/social/social_time2.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
class ProjectionWriter;
class ScopedCheckpointJsonDecode;
struct InteractionCatalog;
struct LocalEconomyCatalog;
struct LocalEconomyState;
inline constexpr const char *public_supply_inspection_source_kind =
    "public_supply_inspected";
// 作者逐一指明既有告示和公开货源，不按同处、物品或组织名称猜测关联。
struct PublicSupplyInspectionCatalog {
  Integer version = 1;
  OrderedTable<StableId> notice_sources;
};
// 只保存实际取得来源，不复制库存、Report 或第二份知识账。
struct PublicSupplyInspection {
  StableId id, actor_id, prop_id, report_id;
  MomentOwnerSource original_observation;
  StableId source_key, settlement_id, place_id;
  WorldMoment inspected_at;
};
struct PublicSupplyInspectionState {
  Integer version = 1;
  OrderedTable<PublicSupplyInspection> results;
};
// 全部引用由同一个完整世界 scope 保活；当前钟不能由调用者提供任意时刻。
struct PublicSupplyInspectionReadContext {
  const CommonStore &common;
  const SocialState &social;
  const InteractionCatalog &interactions;
  const LocalEconomyCatalog &economy_catalog;
  const LocalEconomyState &economy;
  const PublicSupplyInspectionCatalog &catalog;
  const PublicSupplyInspectionState &state;
  const CurrentMomentRead &clock;
};
// 目录交叉验证只核作者关联，查询本身不产生观察或知识。
Status
validate_public_supply_inspection_catalog(const PublicSupplyInspectionCatalog &,
                                          const InteractionCatalog &,
                                          const LocalEconomyCatalog &);
// 只有显式绑定的真实同处 Inspect 可读；返回原 Report
// 指针，未有报告或缺映射拒绝。
Outcome<const Report *> preview_public_supply_inspection(
    const PublicSupplyInspectionReadContext &, const ExactResultTimeRead &,
    const ExactResultReadStamp &, const StableId &actor, const StableId &prop);
// 原候选事务独占的实际 writer：重核原预览、写源、再写唯一
// Knowledge，外层失败整候选撤。
Status inspect_public_supply_in_candidate(
    const PublicSupplyInspectionReadContext &, CommonStore &, SocialState &,
    PublicSupplyInspectionState &, const ExactResultTimeRead &,
    const ExactResultReadStamp &, const StableId &actor, const StableId &prop);
// 冷读只反验历史事实，不以今天的位置或岗位替换原读取现场。
Outcome<const PublicSupplyInspection *>
read_public_supply_inspection_source(const PublicSupplyInspectionReadContext &,
                                     const SourceRef &, Integer schema = 1);
Status validate_public_supply_inspection_sources(
    const PublicSupplyInspectionReadContext &);
// 原命令采用只认该事务末尾唯一新增源；前缀不变且不是已知报告的空操作。
Outcome<const PublicSupplyInspection *>
read_public_supply_inspection_command_effect(
    const PublicSupplyInspectionState &before,
    const PublicSupplyInspectionReadContext &after, const StableId &actor,
    const StableId &prop, const WorldMoment &actual);
// P10 只投影真实读取源，显式不可生成 Report；原观察通过 cause 引用保留。
Outcome<ExactOwnerFactRead> project_public_supply_inspection_source(
    const PublicSupplyInspectionReadContext &, const SourceRef &,
    Integer schema = 1);
// 公共 Knowledge 的有限取得校验；源须已由 P10 反读原 S3 owner。
bool is_public_supply_inspection_source(const SourceRef &);
Status validate_public_supply_inspection_knowledge_source(
    const CommonStore &, const Report &, const KnownReport &,
    const SocialSourceTimeBinding &, const ExactResultTimeRead &,
    const ExactResultReadStamp &);
Outcome<PublicSupplyInspectionCatalog>
decode_public_supply_inspection_catalog(const BoundaryValue &);
Outcome<PublicSupplyInspectionState>
decode_public_supply_inspection_state(const BoundaryValue &);
Outcome<PublicSupplyInspectionState>
decode_public_supply_inspection_state(const BoundaryValue &,
                                      ScopedCheckpointJsonDecode &);
Outcome<BoundaryValue>
encode_public_supply_inspection_state(const PublicSupplyInspectionState &);
Status emit_public_supply_inspection_state(ProjectionWriter &,
                                           const PublicSupplyInspectionState &);
Status emit_public_supply_inspection_fact_payloads_fields(
    ProjectionWriter &, const PublicSupplyInspectionState &);
Status visit_public_supply_inspection_checkpoint(CodecVisitor &,
                                                 PublicSupplyInspectionState &);
// 新 record 的空默认值仅含空字符串/表；Moment 的实际 limb 仍由原 allocator
// 计费。


// OrderedTable 复制前精确预留本原 record 的普通字符串字段。

} // namespace hanmo::resident

// A07 / E17.3 原设施工程。专业根不保存第二工量、余额或可用容量。
#pragma once
#include "../economic_facility_sources.hpp"
#include "../economic_work_sources.hpp"
#include "facility_construction_resource_effects.hpp"
#include "resident/kernel/fixed_service_work_period.hpp"

namespace hanmo::resident {
// 首个有限分支是同一 ProductionAsset 的有界升级。材料与开工实付
// 均由作者明确给出；不把旧 EstateBuild 的农田/粮仓等级当作槽位。
struct ConstructionMaterialRequirement {
  StableId item_id;
  Integer quantity = 0;
};
struct ConstructionTerms {
  EconomicRuleRef terms_ref;
  StableId asset_ref, place_id;
  EconomicRuleRef previous_definition_ref, adopted_definition_ref;
  Integer duration_halfdays = 0;
  std::vector<ConstructionMaterialRequirement> materials;
  Integer money_cost = 0;
  StableId payee_wallet_id;
  Integer capacity_units = 0;
  StableId capacity_unit_id;
};
struct FacilityConstructionCatalog {
  Integer schema_version = 1;
  CatalogIdentity identity;
  std::vector<ConstructionTerms> terms;
};

// 开工冻结的原选批与实际 Consume 结果，不是库存或预付款账户。
struct ConstructionConsumedMaterial {
  CargoSelection actual;
  StableId item_id;
  Owner original_owner;
  std::optional<LotPropertiesRef> properties_ref;
};
struct ConstructionStartCost {
  StableId receipt_id, construction_id;
  WorldMoment occurred_at;
  std::vector<ConstructionConsumedMaterial> consumed;
  StableId payer_wallet_id, payee_wallet_id;
  Integer cash_paid = 0;
  // 从原成功操作捕获的有限冷证据；原摘要不能独自证明钱/物已真正扣除。
  ConstructionStartCostEffects actual_resources;
};

// 一个实际 ActorAction 是一个建设 worker；S0 独占其信用和占用。
// authority/body/presence 都保存原采用源，绝不从今日状态回填旧期。
struct FacilityConstruction {
  StableId construction_id, builder_actor_id;
  EconomicRuleRef terms_ref;
  ActorActionWorkBinding execution_binding;
  WorldMoment started_at;
  StableId actual_cost_ref;
  std::vector<MomentOwnerSource> authority_sources;
  ActorConditionHead accepted_condition_head;
  MomentOwnerSource accepted_presence_source;
};
// 专业期间只绑定真正已供的 S0 /2 工期。工量由原 credit 反读，不保存
// completed_halfdays 或 remaining；没有真实期的钟界不能产生本行。
struct ConstructionPeriodResult {
  StableId receipt_id, construction_id;
  WorkPeriodRef2 actual_period;
  WorldMoment occurred_at;
};
// 当前有限工程恰一个 stage（ordinal 0），必须覆盖条款的全部真实期间。
// 完工和专业应用是两件事实；本行不能直接增槽位或清掉未应用来源。
struct ConstructionStageResult {
  StableId result_id, construction_id;
  Integer stage_ordinal = 0;
  WorldMoment occurred_at;
  std::vector<StableId> actual_period_refs;
  StableId actual_cost_ref;
};
struct FacilityConstructionState {
  Integer schema_version = 1;
  CheckpointVector<FacilityConstruction> constructions;
  CheckpointVector<ConstructionStartCost> start_costs;
  CheckpointVector<ConstructionPeriodResult> periods;
  CheckpointVector<ConstructionStageResult> stages;
};

// 固定地址只是身份；消费者仍必须调用原 typed reader 验全部 payload。
SourceRef facility_construction_started_source(const StableId &);
SourceRef facility_construction_cost_source(const StableId &);
SourceRef facility_construction_period_source(const StableId &);
SourceRef facility_construction_stage_source(const StableId &);
} // namespace hanmo::resident

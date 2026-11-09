// 只在完整 World 候选内调用这些具名命令；任一晚失败须丢弃 Common/Naval 整个候选。
#pragma once
#include "naval_battle_state.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
// Naval3同泊地失能船可先建立真实配员占用；原可航预览不消费此口。
Outcome<NavalFormationQuote> preview_naval_dock_repair_formation(const NavalAssetState&,const NavalCatalog&,const NavalFormationObservation&,const NavalFormationRequest&);
Outcome<NavalAssetState> commit_naval_dock_repair_formation(const NavalAssetState&,const NavalCatalog&,const NavalFormationObservation&,const NavalFormationRequest&,const StableId& fleet_id,const StableId& source_id);
Status validate_naval_repair_policy(const NavalCatalog&,const CommonCatalog*);
Outcome<NavalRepairQuote> preview_naval_repair(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const StableId&,const NavalRepairCommand&);
Outcome<NavalRepairReceipt> apply_naval_repair(CommonWrite&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,NavalState&,const NavalCatalog&,const StableId&,const NavalRepairCommand&);
// 仅既有行动的真实相邻半日可推进；本轮已探得接敌的工单须留待实际 contact 安装后中止。
Outcome<std::optional<NavalRepairReceipt>> step_naval_repair(CommonWrite&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,NavalState&,const NavalCatalog&,const StableId& action_id,const DynamicStepInput&);
// 接敌先登记真实来源，再结束该舰原工单；返回失败完成供原对白/计划出口消费。
Outcome<std::vector<NavalRepairReceipt>> interrupt_naval_repairs_for_contact(CommonWrite&,NavalState&,const NavalCatalog&,const StableId& contact_id,const DynamicStepInput&);
// 冷/热停驻识别仅承认本域仍活动的实际 Action，不能将任意非旅行行动当作修船。
const NavalRepairFact* active_naval_repair(const CommonRead&,const NavalState&,const NavalCatalog&,const StableId& fleet_id);
Status validate_naval_repair_active(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&);
// 提案只来自显式作者维修计划，原Director/命令总线再复评并原子执行。
Outcome<std::optional<NavalRepairStart>> next_naval_repair_proposal(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const StableId& actor_id);
class CommonStore;class CurrentMomentRead;class MilitaryFormationSourceRead;
// Original current quote terms, with source-aware command authority and the
// true clock. This never grants historical participation and never starts work.
// existing_action is only the actual already-adopted repair for completion.
Outcome<NavalRepairTerms> read_exact_naval_repair_current_terms(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,
 const NavalState&,const NavalCatalog&,const CurrentMomentRead&,
 const MilitaryFormationSourceRead*,const StableId& actor,const NavalRepairStart&,
 const StableId& existing_action={});

// Reuses the original current terms/equality gate for the real already-adopted
// action before its material/hull/payment write. No historical current fallback.
Status validate_exact_naval_repair_current_terms(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,
 const NavalState&,const NavalCatalog&,const CurrentMomentRead&,
 const MilitaryFormationSourceRead*,const StableId& actual_action,
 const NavalRepairStart&,const NavalRepairTerms& original_terms);

}

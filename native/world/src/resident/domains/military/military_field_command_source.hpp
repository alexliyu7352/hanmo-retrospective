// 同一原还原器读取形成者与后来实际指挥者；纯读不会签发权力、移动人或生成军团。
#pragma once
#include "military_roster_history.hpp"
#include "military_officer_source.hpp"
#include "naval_battle_state.hpp"
namespace hanmo::resident {
class CurrentMomentRead;
Outcome<MilitaryArmyCommandBinding> query_military_army_command_source_at(const CommonStore&,const MilitaryState&,const StableId& army_id,const CurrentMomentRead&,const MilitaryFormationSourceRead* formations=nullptr);
Outcome<MilitaryArmyCommandBinding> query_military_army_command_source(const CommonStore&,const MilitaryState&,const StableId& army_id);
Outcome<MilitaryArmyCommandBinding> query_military_army_command_source_before(const CommonStore&,const MilitaryState&,const StableId& army_id,const StableId& before_fact_id);
Outcome<MilitaryArmyCommandBinding> query_military_army_command_source_before_at(const CommonStore&,const MilitaryState&,const StableId& army_id,const StableId& before_fact_id,const WorldMoment& actual_current,const MilitaryFormationSourceRead* formations=nullptr);
// Shared original reducer, cut by true Common append position. Formation must
// have its real schema 2 completion application before the prefix at its own M.
// Integer-only assignments keep only legal/nonfuture ticks, not invented M.
Outcome<MilitaryArmyCommandBinding> read_military_army_command_source_at_actual_prefix(const CommonStore&,const MilitaryState&,const StableId& army_id,const CommonActualFactPrefix&,const WorldMoment& at,const WorldMoment& actual_now,const MilitaryFormationSourceRead* formations=nullptr,ExactArithmeticBudget={});
// Ordinal-only original source cut. No endpoint occurrence M is claimed.
Outcome<MilitaryArmyCommandBinding> read_military_army_command_source_at_ordinal_prefix(const CommonStore&,const MilitaryState&,const StableId& army_id,const CommonActualFactPrefix&,const WorldMoment& actual_now,const MilitaryFormationSourceRead* formations=nullptr,ExactArithmeticBudget={},const MilitaryActualSourceWindow* window=nullptr);
Outcome<MilitaryArmyCommandBinding> read_military_army_command_source_exact(const CommonStore&,const MilitaryState&,const StableId& army_id,const WorldMoment& actual_now,const std::optional<SourceRef>& before={},const MilitaryFormationSourceRead* formations=nullptr,ExactArithmeticBudget={});
// 已退编舰队只读退编时的末次真实指挥，后来的陆上接任不倒改旧舰历史。
Outcome<MilitaryArmyCommandBinding> query_military_fleet_command_source(const CommonStore&,const MilitaryState&,const NavalFleetRecord&);
// current仅Common稳定tick；历史必须是真Common fact前边界，完整军事/Naval史由调用者先验。
Outcome<MilitaryFieldCommandObservation> query_military_field_command_source(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const NavalState*,MilitaryFieldCommandKind,const StableId& army_id,const StableId& unit_id);
Outcome<MilitaryFieldCommandObservation> query_military_field_command_source_before(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const NavalState*,const MilitaryAccess&,
 MilitaryFieldCommandKind,const StableId& army_id,const StableId& unit_id,Integer observed_tick,const StableId& before_fact_id);
// 签发人和受命人分别证明真实本军现场；水上须与目标亡官同一原船binding。
Outcome<MilitaryFieldCommandContact> query_military_field_contact(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const NavalState*,const MilitaryFieldCommandTarget&,const StableId& actor_id);
Outcome<MilitaryFieldCommandContact> query_military_field_contact_before(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const NavalState*,const MilitaryAccess&,const MilitaryFieldCommandTarget&,const StableId& actor_id,Integer tick,const StableId& before_fact_id);
// 热战术已有完整World历史前置；只选择原当前Army已生效的末次具名接任，不是新的政治授权口。
const MilitaryFieldCommandFact* current_military_field_army_assignment(const MilitaryState&,const MilitaryCatalog&,const StableId& army_id,const StableId& actor_id);
// 原军令失败不覆盖后来实际接任者的战斗责任；此读不恢复原军令目标或签发新权力。
StableId military_campaign_battle_stop_reason(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const CampaignRecord&);
// 固定枚举映射只表达来源，不授予新的冷版本能力。
MilitaryFieldCommandSource military_field_source(const MilitaryUnitCommandSource&);
}

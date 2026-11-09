// 接舷的Common/Travel/军旅和相关责任共用原候选；边界拒绝不得隐式取消欠债或占用。
#include "naval_integration.hpp"
#include "world_bundle.hpp"
#include "resident/domains/military/naval_capture_source.hpp"
#include "resident/domains/military/naval_npc_settlement.hpp"
#include "resident/domains/military/naval_codec.hpp"
#include "resident/domains/military/naval_repair.hpp"
#include "resident/domains/military/naval_repair_history.hpp"
#include "resident/domains/family/historical_player.hpp"
#include "resident/domains/local_economy/takeover/public_account_dependencies.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
// 原经济/任务责任不能借军事俘获伪装为政治继受，未支持的关联明确拒绝整次接管。
Status capture_responsibilities(const WorldBundle& w,const NavalCaptureFact& fact,const ResourceRecipe& recipe){
 std::set<StableId> inventories(fact.emptied_inventory_ids.begin(),fact.emptied_inventory_ids.end());for(const auto& op:recipe.operations)if(const auto* owner=std::get_if<SetInventoryOwner>(&op))inventories.insert(owner->inventory_id);
 if(inventories.empty())return common_failure("NavalCaptureAssetBoundaryRequired");EconomicPublicAccountSelection selection;selection.original_owner=fact.from_owner;selection.inventory_ids={inventories.begin(),inventories.end()};
 if(w.dynamic_enabled()){auto dependencies=read_economic_public_account_dependencies(w.common,w.local_economy,*w.catalog->local_economy,w.fiscal_settlement_enabled()?&w.fiscal_settlement:nullptr,w.public_relief_enabled()?&w.public_relief:nullptr,selection);if(!dependencies.ok)return {false,{},dependencies.error};const auto& d=dependencies.value;if(!d.production.empty()||!d.obligations.empty()||!d.fiscal.empty()||!d.relief.empty()||!d.supply_source_keys.empty())return common_failure("NavalCaptureEconomicResponsibilityPending",fact.vessel_id);
  for(const auto& row:w.affairs.needs.rows())if(inventories.count(row.second.target_inventory_id))return common_failure("NavalCaptureNeedResponsibilityPending",row.first);
  for(const auto& row:w.catalog->affairs->policies.rows())if(inventories.count(row.second.inventory_id))return common_failure("NavalCaptureNeedResponsibilityPending",row.first);
 }
 for(const auto& row:w.tasks.contracts.rows())if(inventories.count(row.second.frozen_terms.target_inventory_id))return common_failure("NavalCaptureTaskResponsibilityPending",row.first);
 for(const auto& row:w.tasks.shipments.rows())if(inventories.count(row.second.source_inventory_id)||inventories.count(row.second.cargo_inventory_id)||inventories.count(row.second.destination_inventory_id))return common_failure("NavalCaptureTaskResponsibilityPending",row.first);
 return common_success();
}
}
// 两个边界都从真实候选取源；没有关联责任才允许原船舱Owner和明示货批一次转移。
NavalBattlePreviewAccess naval_preview_access(const WorldBundle& w){NavalBattlePreviewAccess out;out.journey_termination_available=true;out.capture_boundary_available=true;out.inspect_capture=[&w](const NavalCaptureFact& fact,const ResourceRecipe& recipe){return capture_responsibilities(w,fact,recipe);};return out;}
// 实际执行重新绑定可写回调，纯预览不取得修改World的句柄。
NavalBattleCommandAccess naval_command_access(WorldBundle& w){NavalBattleCommandAccess out;static_cast<NavalBattlePreviewAccess&>(out)=naval_preview_access(w);
 out.end_journey=[&w](const StableId& id){const auto* action=w.common.action(id);if(!action||action->kind!="travel"||!w.travel.actions.find(id))return common_failure("NavalJourneyTerminationSourceRequired",id);return release_action(w,id);};
 out.capture_boundary.before_transfer=[&w](const NavalCaptureFact& fact,const ResourceRecipe& recipe){return capture_responsibilities(w,fact,recipe);};
 out.capture_boundary.after_transfer=[&w](const NavalCaptureFact& fact){auto access=w.services.military_access();auto source=naval_capture_applied_in_candidate(w.common,w.military,access,w.naval,fact.id);if(!source.ok)return Status{false,{},source.error};if(w.population_military_enabled()){auto context=w.population_write();auto reconciled=reconcile_population_military(context,w.common.tick());if(!reconciled.ok)return Status{false,{},reconciled.error};}return common_success();};return out;
}
// 历史玩家与Travel整数几何均调用各唯一域的原读口，冷档不从当前玩家或frame猜来源。
NavalHistoryAccess naval_history_access(const WorldBundle& w){NavalHistoryAccess out;out.player_before=[&w](Integer tick,const StableId& fact){return w.family_legacy_enabled()?family_player_before_fact_with_legacy(w.common,w.family,w.family_legacy,tick,fact):family_player_before_fact(w.common,w.family,tick,fact);};out.terrain_geometry=[&w](const PointI& start,const TerrainTravelParams& p,bool moving,const StableId& actor,const StableId& action){return terrain_geometry_motion(*w.catalog->travel,w.services,start,p,moving,actor,action);};if(w.population_military_enabled()){out.population=&w.population_military;out.population_catalog=w.catalog->population_military.get();}return out;}
// NPC失败只在真实savepoint回滚之后生成同claim明确放弃；系统继续不冒充任一玩家。
Status run_naval_settlement_decisions(WorldBundle& w){if(!w.naval_combat_enabled())return common_success();std::size_t limit=w.naval.battle.battles.size()+1;for(const auto& r:w.naval.battle.rounds)limit+=r.claims.size()*2;std::vector<std::pair<StableId,StableId>> failed;
 for(std::size_t i=0;i<limit;++i){auto intent=next_naval_npc_settlement(w.common,w.military,*w.catalog->military,w.naval,*w.catalog->naval,w.common.state().metadata.player_actor_id,failed);if(!intent.ok)return {false,{},intent.error};if(!intent.value)return common_success();const auto& next=*intent.value;
  if(next.system_continuation){const auto* command=std::get_if<NavalContinueCommand>(&next.command);if(!command||!next.actor_id.empty())return common_failure("InvalidNavalSystemContinuation");auto access=naval_command_access(w);auto result=apply_naval_continue(w.common,w.military,w.catalog->military->battle_catalog,w.naval,*w.catalog->naval,{},*command,access.end_journey);if(!result.ok)return {false,{},result.error};continue;}
  NavalBattleCommand naval;std::visit([&](const auto& c){naval=c;},next.command);CommandPayload command{naval_battle_command_codec::command_kind(naval),std::move(naval)};bool applied=false;auto result=try_npc_command(w,next.actor_id,command,applied);if(!result.ok)return result;if(applied)continue;const auto* capture=std::get_if<NavalCaptureCommand>(&next.command);if(!capture)return common_failure("NavalNpcSettlementBlocked",next.battle_id);failed.emplace_back(capture->round_source_id,capture->vessel_id);
 }
 return common_failure("NavalSettlementLimitExceeded",{},ErrorKind::internal);
}
// 与原舟战NPC同一有限命令入口，不从舰队损伤自动创造材料或维修意愿。
Status run_naval_repair_decisions(WorldBundle& w){if(!w.naval_repairs_enabled())return common_success();std::set<StableId> seen;for(const auto& plan:w.catalog->naval->repair_policy->npc_plans){if(!seen.insert(plan.actor_id).second)continue;auto ia=w.services.interaction_access();auto ma=w.services.military_access();ma.interaction_access=&ia;auto proposal=next_naval_repair_proposal(w.common,w.military,*w.catalog->military,ma,w.naval,*w.catalog->naval,plan.actor_id);if(!proposal.ok)return {false,{},proposal.error};if(!proposal.value)continue;bool applied=false;CommandPayload command{"naval_repair_start",NavalRepairCommand{*proposal.value}};auto checked=try_npc_command(w,plan.actor_id,command,applied);if(!checked.ok)return checked;}return common_success();}
// 缓存只是永久事实的引用；无缓存history、新kind错tag及旧Naval1都必须严格拒绝。
Status validate_naval_command_history(const WorldBundle& w){const Integer version=w.naval_enabled()?naval_codec::version(*w.catalog->naval):0;const auto& b=w.naval.battle;if(version!=4&&version!=5&&version!=6&&(!w.naval.repair_charges.charges.empty()||!w.naval.repair_charges.field_order.empty()))return common_failure("NavalRepairChargeProfileRequired");if(version!=2&&version!=3&&version!=4&&version!=5&&version!=6&&(!b.battles.empty()||!b.contacts.empty()||!b.plans.empty()||!b.rounds.empty()||!b.captures.empty()||!b.declines.empty()||!b.continuations.empty()||!b.field_order.empty()||!w.naval.field_order.empty()))return common_failure("NavalCombatProfileRequired");auto checked=naval_battle_command_codec::validate_history_version(w.common.state(),version);if(!checked.ok)return checked;
 for(const auto& history:w.common.state().history)if((history.kind=="naval_repair_start"||history.kind=="naval_repair_cancel")&&(version!=3&&version!=4&&version!=5&&version!=6))return common_failure("NavalRepairProfileRequired",history.result_id);
 for(const auto& row:w.common.state().results.rows()){if(!row.second)continue;const auto& r=*row.second;const bool repair_kind=r.kind=="naval_repair_start"||r.kind=="naval_repair_cancel";const auto* repair=r.details?std::get_if<NavalRepairReceipt>(&r.details->data):nullptr;if(!repair_kind&&!repair)continue;if((version!=3&&version!=4&&version!=5&&version!=6)||!repair_kind||!repair)return common_failure("InvalidNavalRepairReceipt",row.first);auto bound=validate_naval_repair_cached_receipt(w.common,w.naval,*w.catalog->naval,row.first,r.kind,r.result_id,r.tick,*repair);if(!bound.ok)return bound;}
 for(const auto& row:w.common.state().results.rows()){if(!row.second)continue;const auto& result=*row.second;const bool kind=naval_battle_command_codec::command_kind(result.kind);const auto* receipt=result.details?std::get_if<NavalBattleCommandReceipt>(&result.details->data):nullptr;if(!kind&&!receipt)continue;if(!kind||!receipt||receipt->kind!=result.kind)return common_failure("InvalidNavalBattleReceipt",row.first);StableId actor;bool found=false;for(const auto& h:w.common.state().history)if(h.result_id==result.result_id){if(found)return common_failure("NavalCachedHistoryMismatch",row.first);found=true;actor=h.actor_id;}if(!found)return common_failure("NavalCachedHistoryMismatch",row.first);checked=naval_battle_command_codec::validate_cached_receipt(w.naval.battle,*receipt,actor,version);if(!checked.ok)return checked;}
 return common_success();
}
}

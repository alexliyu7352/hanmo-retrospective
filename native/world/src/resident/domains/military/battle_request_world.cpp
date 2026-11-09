#include "resident/domains/military/military_model.hpp"
// 同一世界候选中裁定并提交一条AI响应与回执；没有额外时钟、AI世界或资源账户。
#include "battle_request_world.hpp"
#include "battle_world.hpp"
#include "battle_request_codec.hpp"
#include "resident/model/common_operations.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
// 指针仅借当前同一候选，不跨发布保留。
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
// battle.units以unit_id为键，不把军团或向量序号当本人单位。
const BattleUnit* unit(const MilitaryBattleRecord& battle,const StableId& id){for(const auto& row:battle.units)if(row.unit_id==id)return &row;return nullptr;}
// 业务订单比较不受JSON原字段显示顺序影响。
bool same_order(const MilitaryOrder& a,const MilitaryOrder& b){return std::tie(a.action,a.destination.q,a.destination.r,a.heading,a.target_id,a.tactic_id)==std::tie(b.action,b.destination.q,b.destination.r,b.heading,b.target_id,b.tactic_id);}
// 请求拒绝和损坏状态不同：此出口仅表示命令不合法或历史不可发布。
template<class T>Outcome<T> failure(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
// 幂等重放先定位既有回执；新请求才重新检查当前军职，避免重试干涉新回合。
Outcome<MilitaryRequestTransition> prepare(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& access,const StableId& actor,const MilitaryBattleRequest& request){
 if(!request.fields_valid)return failure<MilitaryRequestTransition>("InvalidMilitaryRequest");
 if(!cat.request_policy)return failure<MilitaryRequestTransition>("MilitaryRequestPolicyRequired");
 if(actor!=access.player_actor_id)return failure<MilitaryRequestTransition>("MilitaryCommanderUnauthorized");
 const auto* battle=find(s.battles,request.battle_id);if(!battle)return failure<MilitaryRequestTransition>("UnknownMilitaryBattle");
 bool replay=false;for(const auto& row:s.request_facts)if(row.actor_id==actor&&row.request.battle_id==request.battle_id&&row.request.request_key==request.request_key)replay=true;
 MilitaryRequestAuthority authority;MilitaryDamageSources sources;
 if(!replay){auto scope=military_request_authority(c,s,cat,access,actor,request);if(!scope.ok)return {false,{},scope.error};authority=std::move(scope.value);auto damage=military_damage_sources(s,cat,*battle);if(!damage.ok)return {false,{},damage.error};sources=std::move(damage.value);}
 return prepare_military_request_transition(*battle,cat.battle_catalog,request,actor,authority,*cat.request_policy,s.request_facts,"military_request:preview",c.tick(),sources);
}
}
// 报价只返有限权源/请求许可；编码层不公开裁定前的AI计划或代玩家自动提交。
Outcome<MilitaryQuote> preview_military_request(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& access,const StableId& actor,const MilitaryBattleRequest& request){auto prepared=prepare(c,s,cat,access,actor,request);if(!prepared.ok)return {false,{},prepared.error};MilitaryQuote quote;quote.battle_id=request.battle_id;quote.request_receipt=std::move(prepared.value.receipt);quote.request_replayed=prepared.value.replay;return {true,std::move(quote),{}};}
// 外层World候选统一回滚；所有可预见拒绝先于ID/事实/AI写入，重放不分配新ID。
Outcome<MilitaryEffect> apply_military_request(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& access,const StableId& actor,const MilitaryBattleRequest& request){
 auto prepared=prepare(c,s,cat,access,actor,request);if(!prepared.ok)return {false,{},prepared.error};auto next=std::move(prepared.value);
 if(!next.replay){auto id=c.next_id("military_request");if(!id.ok)return {false,{},id.error};next.receipt.id=id.value;auto fact=c.append_fact({id.value,"military_request","military"});if(!fact.ok)return {false,{},fact.error};for(auto& battle:s.battles)if(battle.id==request.battle_id){battle=std::move(next.battle);break;}s.request_facts.push_back(next.receipt);}
 MilitaryEffect effect;effect.battle_id=request.battle_id;effect.receipt_id=next.receipt.id;effect.already_completed=next.replay;effect.request_answer=military_request_codec::answer(next.receipt);return {true,std::move(effect),{}};
}
// 历史只查当时来源；军职后来更换不能追溯作废旧回执。当前轮另核每次计划替换链。
Status validate_military_requests(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a){
 if(!cat.request_policy)return s.request_facts.empty()?common_success():common_failure("UnexpectedMilitaryRequests");
 if(cat.request_policy->per_actor_round_limit<1)return common_failure("InvalidMilitaryRequestPolicy");
 std::set<StableId> ids;std::set<std::tuple<StableId,StableId,StableId>> keys;std::map<std::tuple<StableId,StableId,Integer>,Integer> counts;std::map<StableId,Integer> last_round;
 for(const auto& row:s.request_facts){
  auto valid=military_request_codec::validate(row);if(!valid.ok)return valid;
  if(row.id.rfind("military_request:",0)!=0||!ids.insert(row.id).second||!keys.emplace(row.actor_id,row.request.battle_id,row.request.request_key).second||++counts[{row.actor_id,row.request.battle_id,row.round}]>cat.request_policy->per_actor_round_limit)return common_failure("InvalidMilitaryRequestHistory",row.id);
  auto battle=find(s.battles,row.request.battle_id);auto army=find(s.armies,row.army_id);auto formation=find(s.action_facts,row.formation_source);auto terms=formation?std::get_if<ArmyFormationTerms>(&formation->terms):nullptr;auto appointment=find(s.action_facts,row.appointment_source);auto office_terms=appointment?std::get_if<OfficeTerms>(&appointment->terms):nullptr;
  if(!battle||!army||!formation||!terms||!c.actor(row.actor_id)||!c.actor(row.commander_actor_id))return common_failure("MilitaryRequestSourceMissing",row.id);
  if(row.tick>c.tick()||row.round>battle->round||row.tick!=battle->created_tick+row.round-1||row.round<last_round[battle->id])return common_failure("MilitaryRequestRoundMismatch",row.id);last_round[battle->id]=row.round;
  if(army->formation_source!=formation->id||formation->command!="army_form"||formation->phase!="completed"||!formation->success||formation->completed_tick>row.tick||formation->actor_id!=terms->actor_id)return common_failure("MilitaryRequestFormationMismatch",row.id);
  bool field_source=false;if(cat.field_command_policy){if(!a.army_command_before)return common_failure("MilitaryFieldCommandReadRequired",row.id);auto controller=a.army_command_before(army->id,row.id);if(!controller.ok)return {false,{},controller.error};if(controller.value.commander_actor_id!=row.commander_actor_id)return common_failure("MilitaryRequestFormationMismatch",row.id);if(controller.value.command_source.kind==MilitaryFieldCommandSourceKind::field_assignment){const auto* f=find(s.field_commands,controller.value.command_source.id);if(!f||f->id!=row.appointment_source||f->office_id!=row.office_id||f->tick>row.tick)return common_failure("MilitaryRequestAppointmentMismatch",row.id);field_source=true;}}else if(terms->actor_id!=row.commander_actor_id||army->commander_actor_id!=row.commander_actor_id)return common_failure("MilitaryRequestFormationMismatch",row.id);
  if(!field_source){if(!appointment||!office_terms||terms->office_id!=row.office_id)return common_failure("MilitaryRequestAppointmentMismatch",row.id);
  if(appointment->command!="office_accept"||appointment->phase!="completed"||!appointment->success||appointment->completed_tick>row.tick||appointment->actor_id!=row.commander_actor_id||office_terms->office_id!=row.office_id)return common_failure("MilitaryRequestAppointmentMismatch",row.id); }

  auto requester=unit(*battle,row.request.unit_id);auto responder=unit(*battle,row.decision.response_unit_id);
  if(!requester||requester->army_id!=row.army_id||requester->commander_actor_id!=row.actor_id||battle->controller_actor_id!=row.actor_id)return common_failure("MilitaryRequestUnitMismatch",row.id);
  if(row.decision.accepted&&(!responder||responder->army_id!=row.army_id||responder->side!=requester->side||responder->commander_actor_id==row.actor_id))return common_failure("MilitaryRequestResponseMismatch",row.id);
  if(row.request.request_kind=="focus_target"){auto target=unit(*battle,row.request.target_unit_id);if(!target||target->side==requester->side)return common_failure("MilitaryRequestTargetMismatch",row.id);}
 }
 for(const auto& battle:s.battles){if(battle.phase!="planning")continue;auto restored=battle;
  for(auto it=s.request_facts.rbegin();it!=s.request_facts.rend();++it){const auto& row=*it;if(row.request.battle_id!=battle.id||row.round!=battle.round||!row.decision.accepted)continue;bool found=false;for(auto& plan:restored.ai_plans)if(plan.first==row.decision.response_unit_id){if(!same_order(plan.second,row.decision.response_order))return common_failure("MilitaryRequestPlanMismatch",row.id);plan.second=*row.previous_order;found=true;}if(!found)return common_failure("MilitaryRequestPlanMismatch",row.id);}
  // 当前轮未改变兵位或时钟，可由回执反向还原AI后正向复核；不读本人草案。
  restored.plan_submitted=false;restored.pending_orders.clear();auto damage=military_damage_sources(s,cat,restored);if(!damage.ok)return {false,{},damage.error};
  for(const auto& row:s.request_facts){if(row.request.battle_id!=battle.id||row.round!=battle.round)continue;std::vector<StableId> responders;for(const auto& plan:restored.ai_plans){auto value=unit(restored,plan.first);if(value&&value->army_id==row.army_id&&value->commander_actor_id!=row.actor_id)responders.push_back(plan.first);}auto decision=consider_military_request(restored,cat.battle_catalog,row.request,responders,damage.value);if(!decision.ok||decision.value.accepted!=row.decision.accepted||decision.value.request_kind!=row.decision.request_kind||decision.value.reason!=row.decision.reason||decision.value.response_unit_id!=row.decision.response_unit_id||(decision.value.accepted&&!same_order(decision.value.response_order,row.decision.response_order)))return common_failure("MilitaryRequestDecisionMismatch",row.id);if(row.decision.accepted)for(auto& plan:restored.ai_plans)if(plan.first==row.decision.response_unit_id)plan.second=row.decision.response_order;}
 }
 return common_success();
}
// 公共缓存的唯一业务来源仍是领域事实，禁止把旧请求答复移植给别人或别场。
Status validate_military_request_cached_answer(const MilitaryState& s,const MilitaryRequestAnswer& answer,const StableId& actor){const auto* receipt=find(s.request_facts,answer.receipt_id);if(!receipt||receipt->actor_id!=actor)return common_failure("MilitaryRequestCachedSourceMissing",answer.receipt_id);const auto original=military_request_codec::answer(*receipt);if(std::tie(answer.receipt_id,answer.battle_id,answer.request_key,answer.request_kind,answer.unit_id,answer.target_unit_id,answer.commander_actor_id,answer.reason,answer.response_unit_id,answer.round,answer.tick,answer.accepted)!=std::tie(original.receipt_id,original.battle_id,original.request_key,original.request_kind,original.unit_id,original.target_unit_id,original.commander_actor_id,original.reason,original.response_unit_id,original.round,original.tick,original.accepted)||answer.response_order.has_value()!=original.response_order.has_value()||(answer.response_order&&!same_order(*answer.response_order,*original.response_order)))return common_failure("MilitaryRequestCachedAnswerMismatch",answer.receipt_id);return common_success();}

}

#include "resident/domains/military/military_model.hpp"
// 玩家请求人不切换为统帅；重放仍核原业务内容，再返回原事实，不重复调用外部转权回调。
#include "naval_battle_commands.hpp"
#include "naval_plan_command.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
template<class T>Outcome<T> fail(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
// preview 只借当前共同序号构造未发布候选，永不调用 next_id 或 append_fact。
Outcome<StableId> candidate_id(const MilitaryAccess& access,const StableId& prefix){if(!access.next_instance_id)return fail<StableId>("NavalInstanceSourceRequired");auto id=access.next_instance_id();if(id<1||id==std::numeric_limits<Integer>::max())return fail<StableId>("InstanceIdExhausted");return {true,prefix+":"+std::to_string(id),{}};}
}
Outcome<NavalBattleCommandQuote> preview_naval_battle_command(const CommonRead& common,const MilitaryState& military,const MilitaryCatalog& commands,const MilitaryAccess& access,const NavalState& naval,const NavalCatalog& ships,const StableId& actor,const NavalBattleCommand& command,const NavalBattlePreviewAccess& application){
 if(!ships.combat_policy)return fail<NavalBattleCommandQuote>("NavalCombatProfileRequired");if(actor.empty())return fail<NavalBattleCommandQuote>("NavalPublicActorRequired");
 return std::visit([&](const auto& c)->Outcome<NavalBattleCommandQuote>{using T=std::decay_t<decltype(c)>;
  if constexpr(std::is_same_v<T,NavalPlanCommand>){auto q=preview_naval_plan(common,military,commands,naval,ships,actor,c);if(!q.ok)return {false,{},q.error};return {true,{naval_battle_command_codec::receipt(q.value.fact),q.value.replayed},{}};}
  else {
   // 幂等事实存在时使用其原 ID，避免 ID 计数已耗尽反而使合法重读失效。
   StableId id;auto original=[&](const auto& rows){for(const auto& f:rows)if(f.actor_id==actor&&f.request_key==c.request_key)id=f.id;};
   if constexpr(std::is_same_v<T,NavalCaptureCommand>)original(naval.battle.captures);else if constexpr(std::is_same_v<T,NavalClaimDeclineCommand>)original(naval.battle.declines);else original(naval.battle.continuations);
   if(id.empty()){auto next=candidate_id(access,std::is_same_v<T,NavalCaptureCommand>?"naval_capture":std::is_same_v<T,NavalClaimDeclineCommand>?"naval_decline":"naval_continue");if(!next.ok)return {false,{},next.error};id=std::move(next.value);}
   if constexpr(std::is_same_v<T,NavalCaptureCommand>){auto q=settle_naval_capture_candidate(common,military,commands,naval,ships,actor,c,id);if(!q.ok)return {false,{},q.error};if(!q.value.replayed){if(!application.inspect_capture||!application.capture_boundary_available)return fail<NavalBattleCommandQuote>("NavalCaptureAssetBoundaryRequired");auto responsibility=application.inspect_capture(q.value.fact,q.value.resources);if(!responsibility.ok)return Outcome<NavalBattleCommandQuote>{false,{},responsibility.error};if(!q.value.interrupted_action_ids.empty()&&!application.journey_termination_available)return fail<NavalBattleCommandQuote>("NavalJourneyTerminationRequired");}return {true,{naval_battle_command_codec::receipt(q.value.fact),q.value.replayed},{}};}
   else if constexpr(std::is_same_v<T,NavalClaimDeclineCommand>){auto q=settle_naval_decline_candidate(common,military,naval,actor,c,id);if(!q.ok)return {false,{},q.error};return {true,{naval_battle_command_codec::receipt(q.value.fact),q.value.replayed},{}};}
   else{auto q=settle_naval_continue_candidate(common,military,commands.battle_catalog,naval,ships,actor,c,id);if(!q.ok)return {false,{},q.error};if(!q.value.replayed&&!q.value.fact.interrupted_action_ids.empty()&&!application.journey_termination_available)return fail<NavalBattleCommandQuote>("NavalJourneyTerminationRequired");return {true,{naval_battle_command_codec::receipt(q.value.fact),q.value.replayed},{}};}
  }
 },command);
}
// apply 重核真实规则，调用者不能把早先的报价对象当作执行授权。
Outcome<NavalBattleCommandReceipt> apply_naval_battle_command(CommonWrite& common,MilitaryState& military,const MilitaryCatalog& commands,const MilitaryAccess& access,NavalState& naval,const NavalCatalog& ships,const StableId& actor,const NavalBattleCommand& command,const NavalBattleCommandAccess& application){
 auto quote=preview_naval_battle_command(common,military,commands,access,naval,ships,actor,command,application);if(!quote.ok)return {false,{},quote.error};if(quote.value.replayed)return {true,quote.value.receipt,{}};
 return std::visit([&](const auto& c)->Outcome<NavalBattleCommandReceipt>{using T=std::decay_t<decltype(c)>;
  if constexpr(std::is_same_v<T,NavalPlanCommand>){auto r=apply_naval_plan(common,military,commands,naval,ships,actor,c);if(!r.ok)return {false,{},r.error};return {true,naval_battle_command_codec::receipt(r.value),{}};}
  else if constexpr(std::is_same_v<T,NavalCaptureCommand>){auto r=apply_naval_capture(common,military,commands,access,naval,ships,actor,c,application.end_journey,application.capture_boundary,application.end_journey_with_source);if(!r.ok)return {false,{},r.error};return {true,naval_battle_command_codec::receipt(r.value),{}};}
  else if constexpr(std::is_same_v<T,NavalClaimDeclineCommand>){auto r=apply_naval_decline(common,military,naval,actor,c);if(!r.ok)return {false,{},r.error};return {true,naval_battle_command_codec::receipt(r.value),{}};}
  else{auto r=apply_naval_continue(common,military,commands.battle_catalog,naval,ships,actor,c,application.end_journey,application.end_journey_with_source);if(!r.ok)return {false,{},r.error};return {true,naval_battle_command_codec::receipt(r.value),{}};}
 },command);
}
}

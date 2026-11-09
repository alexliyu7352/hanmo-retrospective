// 原交易终局只读反查；不会付款、转货、授奖或读取FSL/ActionElapsed。
#include "growth_trade_terminal_read.hpp"
#include "growth_trade_terminal_codec.hpp"
#include <algorithm>
#include <charconv>
#include <string_view>
namespace hanmo::resident {
namespace {
// 原Action分配身份不允许前导零或尚未分配的未来值。
bool allocated_action(const CommonState& common,const StableId& id) {
  constexpr std::string_view prefix="action:";
  if(id.size()<=prefix.size()||id.compare(0,prefix.size(),prefix)!=0)return false;
  const char* first=id.data()+prefix.size();
  if(*first=='0')return false;
  Integer number=0;const auto parsed=std::from_chars(first,id.data()+id.size(),number);
  return parsed.ec==std::errc{}&&parsed.ptr==id.data()+id.size()&&
      number>0&&number<common.next_instance_id;
}
// 历史上下文沿原冻结对象核验，不拿现任provider覆盖过去参与者。
bool same_context(const InteractionContext& a,const InteractionContext& b) {
  return a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&
      a.role_id==b.role_id&&a.kind==b.kind&&a.encounter_id==b.encounter_id&&
      !a.input_error&&!b.input_error;
}
}
Outcome<GrowthTradeTerminalView> read_growth_trade_terminal_v2(
    const CommonStore& common,const GrowthState& growth,const GrowthCatalog& catalog,
    const CommonCatalog& shared,const SourceRef& source,Integer schema,
    const WorldMoment& now,ExactArithmeticBudget budget) {
  auto bad=[&](const char* code)->Outcome<GrowthTradeTerminalView> {
    return {false,{},common_failure(code,source.id).error};
  };
  if(schema!=2||source.domain!="growth"||source.kind!="life_activity"||
     !catalog.regional_life||!catalog.economy||!catalog.complete_study_sources)
    return bad("GrowthTradeTerminalProfileRequired");
  const auto* header=common.state().facts.find(source.id);
  const auto* record=growth.life_activities.find(source.id);
  if(!allocated_action(common.state(),source.id)||!header||header->id!=source.id||
     header->domain!="growth"||header->kind!="life_activity"||!record||
     growth.facts.find(source.id)||
     std::find(common.state().facts.order().begin(),common.state().facts.order().end(),
               source.id)==common.state().facts.order().end())
    return bad("GrowthTradeTerminalOriginalHeaderRequired");
  const auto& view=record->view;
  const auto* terms=std::get_if<TradeTerms>(&record->terms);
  const auto* actor=common.actor(view.actor_id);
  const auto* provider=common.actor(view.provider_actor_id);
  const auto* shop=terms?catalog.shops.find(terms->shop_id):nullptr;
  const auto* role=shared.roles.find(record->context.role_id);
  if(view.kind!=LifeActivityKind::trade||view.fact_id!=source.id||
     view.action_id!=source.id||!terms||!actor||!provider||!shop||!role||
     view.actor_id==view.provider_actor_id||view.participant_actor_ids.size()!=2||
     view.participant_actor_ids[0]!=view.actor_id||
     view.participant_actor_ids[1]!=view.provider_actor_id||
     record->context.provider_actor_id!=view.provider_actor_id||
     record->context.place_id!=role->place_id||record->context.place_id!=shop->place_id||
     record->context.kind||record->context.encounter_id||
     !same_context(record->context,terms->context))
    return bad("GrowthTradeTerminalOriginalIdentityMismatch");
  const bool buy=view.activity=="buy";
  if((!buy&&view.activity!="sell")||view.activity!=terms->side||
     view.item_id!=terms->item_id||!shared.items.find(view.item_id)||
     view.quantity!=terms->quantity||view.quantity<1||view.quantity>500||
     view.amount!=terms->total||view.amount<0||record->cost_paid||record->wage_paid||
     view.start_tick<0||view.start_tick!=terms->valid_tick||
     view.settlement_tick!=view.start_tick||record->selected_trade_settlement||
     view.from_inventory_id!=(buy?shop->inventory_id:actor->inventory_id)||
     view.to_inventory_id!=(buy?actor->inventory_id:shop->inventory_id)||
     view.from_inventory_id==view.to_inventory_id||
     !common.inventory(view.from_inventory_id)||!common.inventory(view.to_inventory_id)||
     view.payer_wallet_id!=(buy?terms->payer_wallet_id:shop->wallet_id)||
     view.payee_wallet_id!=(buy?shop->wallet_id:terms->payer_wallet_id)||
     view.payer_wallet_id==view.payee_wallet_id||
     !common.wallet(view.payer_wallet_id)||!common.wallet(view.payee_wallet_id))
    return bad("GrowthTradeTerminalOriginalTermsMismatch");
  const bool completed=view.phase==LifeActivityPhase::completed;
  if((!completed&&view.phase!=LifeActivityPhase::cancelled)||
     view.success!=completed||(completed?!view.code.empty():view.code.empty())||
     common.action(source.id)||growth.actions.find(source.id))
    return bad("GrowthTradeTerminalOriginalResultRequired");
  auto shape=growth_trade_terminal_codec::validate_activity(*record,true);
  if(!shape.ok)return {false,{},shape.error};
  const auto& terminal=*record->trade_terminal_time;
  auto current=compare_world_moments(now,now,budget);
  if(!current.ok)return {false,{},current.error};
  if(now.half_day_tick<common.tick())return bad("GrowthTradeTerminalClockBeforeWorld");
  auto occurred=compare_world_moments(terminal.ended_at,now,budget);
  if(!occurred.ok)return {false,{},occurred.error};
  if(occurred.value>0)return bad("GrowthTradeTerminalInFuture");
  return {true,{record,&terminal},{}};
}
} // namespace hanmo::resident

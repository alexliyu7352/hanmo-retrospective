// 原terrain_journey与journey_commands的typed移植；每次写入均属于外层唯一候选事务。
#include "travel.hpp"
#include "movement_funded_plan_detail.hpp"
#include <limits>
#include <algorithm>
#include <unordered_set>

namespace hanmo::resident {
namespace {
// 领域首错不修改规则码，能力失败额外分类以禁止NPC continue吞错。
template<class T> Outcome<T> fail(const StableId& code,ErrorKind kind=ErrorKind::rule){return {false,{},{{code,""},kind,{},"travel",{},{}}};}
// 跨结果类型转发原首错，不把普通拒绝改成内部故障。
template<class T,class U> Outcome<T> propagate(const Outcome<U>& result){return {false,{},result.error};}
// 权威坐标仅逐整数相等，不以浮点近似隐藏保存差异。
bool same(PointI a,PointI b){return a.x==b.x&&a.y==b.y;}
// 只接受冻结的三种模式，未知模式明确报错。
Outcome<terrain::Mode> mode(const StableId& value){if(value=="foot")return {true,terrain::Mode::foot,{}};if(value=="army")return {true,terrain::Mode::army,{}};if(value=="boat")return {true,terrain::Mode::boat,{}};return fail<terrain::Mode>("InvalidTravelMode");}
// 解析同候选唯一位置，只识别据点或原野，其他种类不伪装成起点。
Outcome<PointI> actor_point(const TravelCatalog& catalog,const TravelServices& services,const StableId& actor){
    auto location=services.resolved_location(actor);if(!location.ok)return propagate<PointI>(location);
    if(auto* wild=std::get_if<WildernessLocation>(&location.value))return {true,wild->point,{}};
    if(auto* settlement=std::get_if<SettlementLocation>(&location.value)){auto* found=catalog.settlements.find(settlement->settlement_id);if(found)return {true,found->map_xy,{}};}
    return fail<PointI>("InvalidDestination");
}
// 目的地仅为已存在据点或显式整数点；图内通行由几何核对。
Outcome<PointI> destination_point(const TravelCatalog& catalog,const TravelDestination& destination){if(auto* id=std::get_if<StableId>(&destination)){auto* place=catalog.settlements.find(*id);if(!place)return fail<PointI>("InvalidDestination");return {true,place->map_xy,{}};}return {true,std::get<PointI>(destination),{}};}
// 快照只能复用完全相同的权威行旅输入，不能把资源失效的旧轨迹当新结果。
bool same_motion_params(const TerrainTravelParams& a,const TerrainTravelParams& b){if(a.plan_id!=b.plan_id||a.map_id!=b.map_id||a.origin!=b.origin||a.payer_wallet_id!=b.payer_wallet_id||a.mode!=b.mode||a.route_index!=b.route_index||a.budget!=b.budget||a.travel_credit!=b.travel_credit||a.money_per_tick!=b.money_per_tick||a.fatigue_per_tick!=b.fatigue_per_tick||a.paused!=b.paused||a.encounter_id!=b.encounter_id||a.met_actions!=b.met_actions||a.points.size()!=b.points.size()||a.destination.index()!=b.destination.index())return false;for(std::size_t i=0;i<a.points.size();++i)if(!same(a.points[i],b.points[i]))return false;if(auto* id=std::get_if<StableId>(&a.destination))return *id==std::get<StableId>(b.destination);return same(std::get<PointI>(a.destination),std::get<PointI>(b.destination));}
// 原整型二分插值的截断必须逐式保留，不能改成更精确的float线性插值。
PointI partial_point(const terrain::TerrainMap& map,terrain::Mode travel_mode,const terrain::Flags& flags,PointI from,PointI to,Integer budget){
    Integer low=0,high=1000000;PointI result=from;
    for(int iteration=0;iteration<20;++iteration){Integer amount=(low+high+1)/2;PointI candidate{from.x+(to.x-from.x)*amount/1000000,from.y+(to.y-from.y)*amount/1000000};
        auto queried=terrain::segment_cost_prepared(map,from,candidate,travel_mode,flags);
        if(queried.ok&&queried.value.cost<=budget&&terrain::segment_cost_prepared(map,candidate,to,travel_mode,flags).ok){result=candidate;low=amount;}else high=amount-1;
        if(low>=high)break;
    }return result;
}
// 报价中保存的credit只能沿用合法既有值，不能凭改道增加预算。
Integer saved_credit(const TerrainTravelParams* p){return p&&p->travel_credit>=0&&p->travel_credit<=2147483647LL?p->travel_credit:0;}
// 只按真实整数前缀判定行军执行，不凭moving元数据制造条件事实。
bool retained_movement(PointI from,const std::vector<PointI>& points,Integer reached,PointI to){for(Integer i=0;i<reached;++i){if(!same(points[static_cast<std::size_t>(i)],from))return true;from=points[static_cast<std::size_t>(i)];}return !same(to,from);}
} // 私有辅助不持有候选地址或独立资源状态。

// 独行/同行取最大背负；军团按真实活人及fit集合均摊且仅扣公柜。
Outcome<TravelProfile> travel_profile(const CommonRead& common,const TravelServices& services,const StableId& actor_id){
    auto* actor=common.actor(actor_id);if(!actor)return fail<TravelProfile>("UnknownActor");
    auto army=services.active_army(actor_id);if(!army.ok)return propagate<TravelProfile>(army);
    auto fleet=services.vessel_fleet(actor_id);if(!fleet.ok)return propagate<TravelProfile>(fleet);if(fleet.value){const auto& f=*fleet.value;if(!army.value||f.fleet_id.empty()||f.army_id.empty()||f.commander_actor_id!=actor_id||army.value->commander_actor_id!=actor_id||f.payer_wallet_id!=army.value->wallet_id||f.travel_budget<1)return fail<TravelProfile>("NavalTravelProofMismatch");return {true,{"boat",f.payer_wallet_id,0,f.travel_budget},{}};}
    Integer weight=0;
    if(!army.value){auto* members=common.members(actor_id);if(!members)return fail<TravelProfile>("InvalidParty",ErrorKind::internal);
        for(const auto& id:*members){auto* member=common.actor(id);if(!member)return fail<TravelProfile>("UnknownActor",ErrorKind::internal);auto w=common.inventory_weight(member->inventory_id);if(!w.ok)return propagate<TravelProfile>(w);weight=std::max(weight,w.value);}
        return {true,{"foot",actor->wallet_id,weight},{}};
    }
    const auto& a=*army.value;if(a.commander_actor_id!=actor_id)return fail<TravelProfile>("NotArmyCommander");std::unordered_set<StableId> people;
    for(const auto& id:a.member_actor_ids){auto* member=common.actor(id);if(!member)return fail<TravelProfile>("UnknownActor",ErrorKind::internal);if(member->alive){people.insert(id);auto w=common.inventory_weight(member->inventory_id);if(!w.ok)return propagate<TravelProfile>(w);weight+=w.value;}}
    for(const auto& id:a.fit_actor_ids)people.insert(id);
    if(people.empty())return fail<TravelProfile>("MilitaryUnitUnavailable");
    auto w=common.inventory_weight(a.inventory_id);if(!w.ok)return propagate<TravelProfile>(w);weight+=w.value;
    auto count=static_cast<Integer>(people.size());return {true,{"army",a.wallet_id,(weight+count-1)/count},{}};
}
// 按原首错顺序逐路点规划，不读取未来世界或修改任何资源。
static Outcome<TravelQuote> plan_terrain_travel_original(const CommonRead& common,const TravelState& state,const TravelCatalog& catalog,const TravelServices& services,const StableId& actor_id,const TravelDestination& destination,const std::vector<StableId>& via,const StableId* accepted_payer,const StableId* pending_action){auto field=common.field_battle_bound(actor_id);if(!field.ok)return {false,{},field.error};if(field.value)return {false,{},common_failure("FieldBattleMovementBlocked",actor_id).error};
    const auto* map=catalog.maps.find(catalog.world_map_id);if(catalog.world_map_id.empty()||!map)return fail<TravelQuote>("UnknownWorldMap");
    auto* actor=common.actor(actor_id);if(!actor)return fail<TravelQuote>("UnknownActor");auto location=services.resolved_location(actor_id);if(!location.ok)return propagate<TravelQuote>(location);
    if(!std::holds_alternative<SettlementLocation>(location.value)&&!std::holds_alternative<WildernessLocation>(location.value))return fail<TravelQuote>("InvalidDestination");
    if(auto* at=std::get_if<SettlementLocation>(&location.value)){auto* settlement=catalog.settlements.find(at->settlement_id);if(!settlement||at->place_id!=settlement->entry_place)return fail<TravelQuote>("NotAtCityExit");}
    auto* members=common.members(actor_id);if(!members)return fail<TravelQuote>("InvalidParty",ErrorKind::internal);
    for(const auto& id:*members){auto* participant=common.actor(id);if(!participant||!participant->alive||!participant->available)return fail<TravelQuote>("ParticipantBusy");if(!participant->action_id.empty()){auto* current=common.action(participant->action_id);if(!current||current->kind!="travel"||current->actor_id!=actor_id||(!state.actions.find(current->id)&&(!pending_action||current->id!=*pending_action)))return fail<TravelQuote>("ParticipantBusy");}}
    if(via.size()>8)return fail<TravelQuote>("InvalidWaypoints");
    auto target=destination_point(catalog,destination);if(!target.ok)return propagate<TravelQuote>(target);auto origin=actor_point(catalog,services,actor_id);if(!origin.ok)return propagate<TravelQuote>(origin);
    if(same(target.value,origin.value)&&via.empty())return fail<TravelQuote>("AlreadyThere");
    std::vector<PointI> stops;std::unordered_set<StableId> seen;
    for(const auto& id:via){auto* settlement=catalog.settlements.find(id);if(!settlement||!seen.insert(id).second||(std::holds_alternative<StableId>(destination)&&std::get<StableId>(destination)==id))return fail<TravelQuote>("InvalidWaypoints");stops.push_back(settlement->map_xy);}stops.push_back(target.value);
    auto profile=travel_profile(common,services,actor_id);if(!profile.ok)return propagate<TravelQuote>(profile);if(accepted_payer){if(profile.value.mode!="foot")return fail<TravelQuote>("FundedPersonalMovementMustBePersonal");profile.value.payer_wallet_id=*accepted_payer;}auto travel_mode=mode(profile.value.mode);if(!travel_mode.ok)return propagate<TravelQuote>(travel_mode);
    // 首个内河片只能在明确泊地据点抵岸，避免合法抵达后没有显式解编出口。
    if(profile.value.mode=="boat")if(const auto* city=std::get_if<StableId>(&destination)){const auto* stop=catalog.settlements.find(*city);if(!stop||!services.naval_berth(stop->entry_place))return fail<TravelQuote>("NavalDestinationBerthRequired");}
    auto budget_entry=map->budgets.find(travel_mode.value);if(budget_entry==map->budgets.end())return fail<TravelQuote>("InvalidTravelCosts",ErrorKind::internal);
    Integer budget=profile.value.mode=="boat"?std::min(budget_entry->second,profile.value.naval_budget):std::max<Integer>(1,budget_entry->second*100/(100+profile.value.load_weight*4));if(budget<1)return fail<TravelQuote>("NavalTravelProofMismatch");Integer money=map->money_per_tick+services.party_extra_cost(actor_id);
    auto spendable=common.spendable(profile.value.payer_wallet_id);if(!spendable.ok)return propagate<TravelQuote>(spendable);if(spendable.value<money)return fail<TravelQuote>("InsufficientFunds");
    std::vector<PointI> points{origin.value};Integer cost=0;
    for(auto stop:stops){auto route=catalog.route_cache?catalog.route_cache->plan(*map,points.back(),stop,travel_mode.value,services.terrain_flags()):terrain::plan_prepared(*map,points.back(),stop,travel_mode.value,services.terrain_flags());if(!route.ok)return propagate<TravelQuote>(route);points.insert(points.end(),route.value.points.begin()+1,route.value.points.end());cost+=route.value.cost;}
    if(points.size()<2)return fail<TravelQuote>("AlreadyThere");
    Integer credit=saved_credit(state.actions.find(actor->action_id));Integer duration=std::max<Integer>(1,(std::max<Integer>(0,cost-credit)+budget-1)/budget);
    StableId origin_id=std::holds_alternative<WildernessLocation>(location.value)?std::get<WildernessLocation>(location.value).origin:std::get<SettlementLocation>(location.value).settlement_id;
    TravelQuote quote;quote.duration=duration;quote.cost=duration*money;quote.budget=budget;quote.starting_load=profile.value.load_weight;quote.money_per_tick=money;quote.fatigue_per_tick=map->fatigue_per_tick;quote.map_id=catalog.world_map_id;quote.origin=origin_id;quote.mode=profile.value.mode;quote.payer_wallet_id=profile.value.payer_wallet_id;quote.points=std::move(points);quote.destination=destination;if(quote.mode=="boat"){auto admitted=services.naval_voyage_allowed(actor_id,quote);if(!admitted.ok)return propagate<TravelQuote>(admitted);}return {true,std::move(quote),{}};
}
Outcome<TravelQuote> plan_terrain_travel(const CommonRead& common,const TravelState& state,const TravelCatalog& catalog,const TravelServices& services,const StableId& actor_id,const TravelDestination& destination,const std::vector<StableId>& via){return plan_terrain_travel_original(common,state,catalog,services,actor_id,destination,via,nullptr,nullptr);}
Outcome<TravelQuote> movement_funded_plan_detail::terrain(const TaskPersonalMovementFundingContext& x,const SourceRef& plan,const StableId& actor,const TravelDestination& destination,const std::vector<StableId>& via,const StableId& pending){auto funding=accepted_basis(x,plan,actor,destination,pending);if(!funding.ok)return {false,{},funding.error};return plan_terrain_travel_original(x.funding.common,x.travel,x.travel_catalog,x.travel_services,actor,destination,via,&funding.value.payer_wallet_id,pending.empty()?nullptr:&pending);}
// 每次实际推进仍重核成员、前缀通行与spendable，暂停不积攒预算。
Status can_step_terrain_travel(const CommonRead& common,const TravelCatalog& catalog,const TravelServices& services,const ActionCoreView& action,const TerrainTravelParams& p,bool ignore_pause){auto field=common.field_battle_bound(action.actor_id);if(!field.ok)return {false,{},field.error};if(field.value)return {false,{},common_failure("FieldBattleMovementBlocked",action.actor_id).error};
    if(p.paused&&!ignore_pause)return fail<Done>("TravelPaused");
    auto* members=common.members(action.actor_id);if(!members)return fail<Done>("InvalidParty",ErrorKind::internal);
    for(const auto& id:*members){auto* actor=common.actor(id);if(!actor||!actor->alive||!actor->available)return fail<Done>("ParticipantBusy");}
    const auto* map=catalog.maps.find(p.map_id);if(!map)return fail<Done>("UnknownWorldMap",ErrorKind::internal);auto m=mode(p.mode);if(!m.ok)return propagate<Done>(m);auto from=actor_point(catalog,services,action.actor_id);if(!from.ok)return propagate<Done>(from);
    auto fleet=services.vessel_fleet(action.actor_id);if(!fleet.ok)return propagate<Done>(fleet);if(p.mode=="boat"||fleet.value){const auto rate=map->budgets.find(terrain::Mode::boat);if(p.mode!="boat"||!fleet.value||fleet.value->commander_actor_id!=action.actor_id||fleet.value->payer_wallet_id!=p.payer_wallet_id||rate==map->budgets.end()||p.budget!=std::min(rate->second,fleet.value->travel_budget))return fail<Done>("NavalTravelProofMismatch");}
    if(!terrain::point_valid_prepared(*map,from.value,m.value,services.terrain_flags()))return fail<Done>("TerrainBlocked");
    Integer budget=p.budget+p.travel_credit;PointI here=from.value;
    for(auto target:p.points){auto segment=terrain::segment_cost_prepared(*map,here,target,m.value,services.terrain_flags());if(!segment.ok)return fail<Done>("TerrainBlocked");budget-=segment.value.cost;if(budget<=0)break;here=target;}
    auto available=common.spendable(p.payer_wallet_id);if(!available.ok)return propagate<Done>(available);if(available.value<p.money_per_tick)return fail<Done>("Stranded");return common_success();
}
// 一次快照只计算一份真实积分轨迹，零碎预算与整数落点规则逐项保原。
// 同一积分轨迹用于热执行和历史接敌重算；没有另一套舟行取整公式。
Outcome<TravelMotion> terrain_geometry_motion(const TravelCatalog& catalog,const TravelServices& services,const PointI& start,const TerrainTravelParams& p,bool moving,const StableId& actor_id,const StableId& action_id){
    const auto* map=catalog.maps.find(p.map_id);if(!map)return fail<TravelMotion>("UnknownWorldMap",ErrorKind::internal);auto m=mode(p.mode);if(!m.ok)return propagate<TravelMotion>(m);
    if(p.budget<1||p.travel_credit<0||p.travel_credit>std::numeric_limits<Integer>::max()-p.budget)return fail<TravelMotion>("InvalidTravelBudget");
    TravelMotion result;result.actor_id=actor_id;result.action_id=action_id;result.map_id=p.map_id;result.origin=p.origin;result.params=p;
    result.budget=p.budget+p.travel_credit;result.moving=moving;Integer consumed=0,reached=0;PointI here=start;
    if(moving)while(reached<static_cast<Integer>(p.points.size())){Integer available=result.budget-consumed;if(available<=0)break;PointI target=p.points[static_cast<std::size_t>(reached)];auto segment=terrain::segment_cost_prepared(*map,here,target,m.value,services.terrain_flags());if(!segment.ok)return propagate<TravelMotion>(segment);Integer cost=segment.value.cost;if(cost==0){++reached;continue;}
        PointI landing=cost<=available?target:partial_point(*map,m.value,services.terrain_flags(),here,target,available);if(same(landing,here))break;auto profile=terrain::segment_profile_prepared(*map,here,landing,m.value,services.terrain_flags());if(!profile.ok)return propagate<TravelMotion>(profile);Integer spent=profile.value.cost;
        for(const auto& s:profile.value.spans){result.segments.push_back({{s.from.x,s.from.y},{s.to.x,s.to.y},consumed+s.start_cost,consumed+s.end_cost,reached});result.cost_piece_kinds.push_back(TerrainCostPieceKind::moving_cost);}
        if(profile.value.raw_cost<static_cast<double>(spent)){result.cost_piece_kinds.push_back(TerrainCostPieceKind::rounding_padding);result.segments.push_back({{static_cast<double>(landing.x),static_cast<double>(landing.y)},{static_cast<double>(landing.x),static_cast<double>(landing.y)},consumed+profile.value.raw_cost,static_cast<double>(consumed+spent),same(landing,target)?reached+1:reached});}
        consumed+=spent;here=landing;if(same(here,target))++reached;
    }
    if(consumed<result.budget){result.cost_piece_kinds.push_back(TerrainCostPieceKind::unused_budget_tail);result.segments.push_back({{static_cast<double>(here.x),static_cast<double>(here.y)},{static_cast<double>(here.x),static_cast<double>(here.y)},static_cast<double>(consumed),static_cast<double>(result.budget),reached});}
    result.actual_consumed_total=consumed;result.original_point_count=static_cast<Integer>(p.points.size());result.end_credit=moving?(reached<static_cast<Integer>(p.points.size())?result.budget-consumed:0):p.travel_credit;result.end=here;result.reached_points=reached;return {true,std::move(result),{}};
}
Outcome<TravelMotion> terrain_motion(const CommonRead& common,const TravelCatalog& catalog,const TravelServices& services,const ActionCoreView& action,const TerrainTravelParams& p,const std::vector<StableId>& hold){
    auto start=actor_point(catalog,services,action.actor_id);if(!start.ok)return propagate<TravelMotion>(start);auto* map=catalog.maps.find(p.map_id);if(!map)return fail<TravelMotion>("UnknownWorldMap",ErrorKind::internal);auto m=mode(p.mode);if(!m.ok)return propagate<TravelMotion>(m);
    bool moving=false;if(std::find(hold.begin(),hold.end(),action.actor_id)==hold.end()){auto allowed=can_step_terrain_travel(common,catalog,services,action,p);if(!allowed.ok&&allowed.error.kind!=ErrorKind::rule)return propagate<TravelMotion>(allowed);moving=allowed.ok;}
    auto geometry=terrain_geometry_motion(catalog,services,start.value,p,moving,action.actor_id,action.id);if(!geometry.ok)return geometry;auto result=std::move(geometry.value);
    result.read_versions.push_back(common.read_version({ResourceKey::Kind::action,action.id}));result.read_versions.push_back(common.read_version({ResourceKey::Kind::wallet,p.payer_wallet_id}));
    if(auto* group=common.members(action.actor_id))for(const auto& member:*group)result.read_versions.push_back(common.read_version({ResourceKey::Kind::actor,member}));
    auto party=services.party_id(action.actor_id);if(!party.empty())result.read_versions.push_back(common.read_version({ResourceKey::Kind::party,party}));
    return {true,std::move(result),{}};
}
// 安装原计划事实和剩余后缀；尚未走的路不提前收费。
static Status begin_terrain_travel_without_capture(CommonWrite& common,TravelState& state,StableId action_id,const TravelQuote& quote){if(const auto* planned=common.action(action_id)){auto field=common.field_battle_bound(planned->actor_id);if(!field.ok)return {false,{},field.error};if(field.value)return common_failure("FieldBattleMovementBlocked",planned->actor_id);}
    const auto* borrowed=common.action(action_id);if(!borrowed||borrowed->kind!="travel"||quote.points.size()<2)return fail<Done>("InvalidTerrainJourney",ErrorKind::internal);auto action=*borrowed;auto old=state.actions.find(action_id);Integer credit=saved_credit(old);std::vector<StableId> met=old?old->met_actions:std::vector<StableId>{};
    auto id=common.next_id("journey");if(!id.ok)return propagate<Done>(id);auto appended=common.append_fact({id.value,"terrain_journey","travel"});if(!appended.ok)return appended;
    TerrainJourneyFact fact;fact.id=id.value;fact.actor_id=action.actor_id;fact.action_id=action.id;fact.map_id=quote.map_id;fact.mode=quote.mode;fact.tick=common.tick();fact.budget=quote.budget;fact.starting_load=quote.starting_load;fact.origin=quote.points.front();fact.destination=quote.destination;fact.points=quote.points;state.journey_facts.set(id.value,std::move(fact));
    TerrainTravelParams p;p.plan_id=id.value;p.map_id=quote.map_id;p.origin=quote.origin;p.payer_wallet_id=quote.payer_wallet_id;p.mode=quote.mode;p.budget=quote.budget;p.travel_credit=credit;p.money_per_tick=quote.money_per_tick;p.fatigue_per_tick=quote.fatigue_per_tick;p.points.assign(quote.points.begin()+1,quote.points.end());p.destination=quote.destination;p.met_actions=std::move(met);state.actions.set(action_id,std::move(p));
    ActionMutation mutation;mutation.action=action;mutation.remaining_ticks=std::max<Integer>(1,quote.duration);auto changed=common.apply_action(mutation);if(!changed.ok)return changed;ActorMutation actor;actor.actor_id=action.actor_id;actor.location=WildernessLocation{quote.map_id,quote.origin,quote.points.front()};return common.apply_actor(actor);
}
// 原begin全部成功后才输出真实前后计划；草稿不分ID、不提前计费。
Status begin_terrain_travel(CommonWrite& common,TravelState& state,StableId id,const TravelQuote& quote,std::optional<TravelPlanInstallationDraft>* installation){
    if(!installation)return begin_terrain_travel_without_capture(common,state,id,quote);installation->reset();
    auto before=capture_original_travel_plan_snapshot(common,state,id);if(!before.ok)return {false,{},before.error};
    TravelPlanInstallationDraft draft;draft.source_tick=common.tick();draft.original_quote=quote;draft.before=std::move(before.value);
    auto applied=begin_terrain_travel_without_capture(common,state,id,quote);if(!applied.ok)return applied;
    auto after=capture_original_travel_plan_snapshot(common,state,id);if(!after.ok)return {false,{},after.error};if(!after.value.params)return common_failure("TravelPlanCaptureInstalledPayloadRequired",id);
    draft.after=std::move(after.value);*installation=std::move(draft);return common_success();
}
// 唯一真实step扣一次钱包和疲劳；结束判断由中央结合contacted决定是否释放行动。
Outcome<TravelStepResult> step_terrain_travel(CommonWrite& common,TravelState& state,const TravelCatalog& catalog,const TravelServices& services,StableId action_id,bool capture,const TravelMotion* snapshot,std::optional<TravelStepExpenseDraft>* expense){
    if(expense)expense->reset();
    auto* borrowed=common.action(action_id);auto* params=state.actions.find(action_id);if(!borrowed||!params)return fail<TravelStepResult>("InvalidTerrainJourney",ErrorKind::internal);auto action=*borrowed;auto p=*params;
    auto allowed=can_step_terrain_travel(common,catalog,services,action,p);if(!allowed.ok)return propagate<TravelStepResult>(allowed);bool reusable=snapshot&&snapshot->moving&&snapshot->actor_id==action.actor_id&&snapshot->action_id==action.id&&!snapshot->read_versions.empty()&&same_motion_params(snapshot->params,p);
    if(reusable)for(const auto& version:snapshot->read_versions)if(common.read_version(version.resource).version!=version.version){reusable=false;break;}
    if(reusable){auto army=services.active_army(action.actor_id);if(!army.ok)return propagate<TravelStepResult>(army);if(army.value)reusable=false;}
    auto trace=reusable?Outcome<TravelMotion>{true,*snapshot,{}}:terrain_motion(common,catalog,services,action,p);if(!trace.ok)return propagate<TravelStepResult>(trace);auto from=actor_point(catalog,services,action.actor_id);if(!from.ok)return propagate<TravelStepResult>(from);
    // 原step仅就地写location.point；先拥有实际位置，不能在共享资源写入后继续借用actor。
    const auto* moving_actor=common.actor(action.actor_id);if(!moving_actor||!std::holds_alternative<WildernessLocation>(moving_actor->location))return fail<TravelStepResult>("InvalidTerrainJourneyActor",ErrorKind::internal);
    // 仅拥有本次写前快照，草稿直到全部原写成功才交给调用者。
    std::optional<TravelStepExpenseDraft> draft;if(expense){draft.emplace();draft->action=action;draft->source_tick=common.tick();draft->before_location=moving_actor->location;draft->before_params=p;}
    Location location=moving_actor->location;std::get<WildernessLocation>(location).point=trace.value.end;
    TravelStepResult result;if(capture&&(p.mode=="boat"||retained_movement(from.value,p.points,trace.value.reached_points,trace.value.end))){TerrainExecution e;e.actor_id=action.actor_id;e.action_id=action.id;e.plan_id=p.plan_id;e.map_id=p.map_id;e.action_start_tick=action.start_tick;e.source_tick=common.tick();e.route_index_before=p.route_index;e.route_index_after=p.route_index+trace.value.reached_points;e.travel_credit_before=p.travel_credit;e.travel_credit_after=trace.value.end_credit;e.from_point=from.value;e.to_point=trace.value.end;result.execution=e;}
    ResourceRecipe recipe;recipe.operations.push_back(WalletDelta{p.payer_wallet_id,-p.money_per_tick,true});auto paid=common.apply_resources(recipe);if(!paid.ok)return propagate<TravelStepResult>(paid);if(draft)draft->applied_payment=std::get<WalletDelta>(recipe.operations.front());
    auto* group=common.members(action.actor_id);if(!group)return fail<TravelStepResult>("InvalidParty",ErrorKind::internal);auto members=*group;
    for(const auto& id:members){const auto* a=common.actor(id);if(!a)return fail<TravelStepResult>("UnknownActor",ErrorKind::internal);
        std::optional<TravelStepFatigueWrite> condition;if(draft)condition=TravelStepFatigueWrite{id,a->hp,0,a->fatigue,0,p.fatigue_per_tick};
        ActorMutation change;change.actor_id=id;change.fatigue=std::min<Integer>(100,a->fatigue+p.fatigue_per_tick);auto wrote=common.apply_actor(change);if(!wrote.ok)return propagate<TravelStepResult>(wrote);
        if(condition){const auto* after=common.actor(id);if(!after)return fail<TravelStepResult>("UnknownActor",ErrorKind::internal);condition->hp_after=after->hp;condition->fatigue_after=after->fatigue;draft->fatigue_writes.push_back(std::move(*condition));}
    }
    p.points.erase(p.points.begin(),p.points.begin()+trace.value.reached_points);p.route_index+=trace.value.reached_points;p.travel_credit=trace.value.end_credit;Integer remaining=0;PointI here=trace.value.end;const auto* map=catalog.maps.find(p.map_id);auto m=mode(p.mode);if(!map||!m.ok)return fail<TravelStepResult>("InvalidTerrainJourney",ErrorKind::internal);
    for(auto target:p.points){auto segment=terrain::segment_cost_prepared(*map,here,target,m.value,{});if(!segment.ok)return propagate<TravelStepResult>(segment);remaining+=segment.value.cost;here=target;}
    ActionMutation remaining_change;remaining_change.action=action;remaining_change.remaining_ticks=std::max<Integer>(1,(std::max<Integer>(0,remaining-p.travel_credit)+p.budget-1)/p.budget);auto changed=common.apply_action(remaining_change);if(!changed.ok)return propagate<TravelStepResult>(changed);
    result.completed=p.points.empty();if(result.completed&&std::holds_alternative<StableId>(p.destination)){const auto& id=std::get<StableId>(p.destination);const auto* settlement=catalog.settlements.find(id);if(!settlement)return fail<TravelStepResult>("InvalidDestination",ErrorKind::internal);location=SettlementLocation{id,settlement->entry_place};}
    ActorMutation actor_change;actor_change.actor_id=action.actor_id;actor_change.location=location;if(std::holds_alternative<WildernessLocation>(location))actor_change.location_mode=ActorMutation::LocationMode::wilderness_point;auto relocated=common.apply_actor(actor_change);if(!relocated.ok)return propagate<TravelStepResult>(relocated);if(draft){draft->after_params=p;draft->after_location=location;draft->completed=result.completed;}state.actions.set(action_id,std::move(p));if(result.execution)result.execution->result_location=location;if(expense)*expense=std::move(draft);return {true,std::move(result),{}};
}
// 原NotTravelling优先于ActorUnavailable；五个途中命令不创建新行动或时钟。
Outcome<PreparedTravelOp> prepare_travel_control(const CommonRead& common,const TravelState& state,const TravelCatalog& catalog,const TravelServices& services,const StableId& actor_id,const TravelCommandPayload& command){auto field=common.field_battle_bound(actor_id);if(!field.ok)return {false,{},field.error};if(field.value)return {false,{},common_failure("FieldBattleMovementBlocked",actor_id).error};
    auto* actor=common.actor(actor_id);auto* action=actor?common.action(actor->action_id):nullptr;if(!actor||!action||action->kind!="travel"||(!std::holds_alternative<WildernessLocation>(actor->location)&&!std::holds_alternative<OnRouteLocation>(actor->location)))return fail<PreparedTravelOp>("NotTravelling");if(!actor->alive||!actor->available)return fail<PreparedTravelOp>("ActorUnavailable");
    if(!state.actions.find(action->id)&&!state.road_actions.find(action->id))return fail<PreparedTravelOp>("InvalidTravelAction",ErrorKind::internal);
    PreparedTravelOp op;op.actor_id=actor_id;op.command=command;
    if(std::holds_alternative<ResumeTravelCommand>(command)){auto allowed=can_step_travel(common,state,catalog,services,*action,true);if(!allowed.ok)return propagate<PreparedTravelOp>(allowed);}
    else if(std::holds_alternative<PauseTravelCommand>(command)||std::holds_alternative<TravelWaitCommand>(command)){}
    else {TravelDestination destination;std::vector<StableId> via;if(std::holds_alternative<ReturnTravelCommand>(command)){if(auto* wild=std::get_if<WildernessLocation>(&actor->location))destination=wild->origin;else destination=std::get<OnRouteLocation>(actor->location).origin;}else if(auto* reroute=std::get_if<RerouteCommand>(&command)){destination=reroute->destination;via=reroute->via;}else return fail<PreparedTravelOp>("UnsupportedAction");auto quote=std::holds_alternative<OnRouteLocation>(actor->location)?replan_road_travel(common,catalog,services,actor_id,destination,via):plan_terrain_travel(common,state,catalog,services,actor_id,destination,via);if(!quote.ok)return propagate<PreparedTravelOp>(quote);op.quote=std::move(quote.value);}
    return {true,std::move(op),{}};
}
// 控制apply只修改当前候选，travel_wait的半日推进由中央处理hold本人。
static Status apply_travel_control_without_capture(CommonWrite& common,TravelState& state,const TravelCatalog& catalog,const PreparedTravelOp& op){auto field=common.field_battle_bound(op.actor_id);if(!field.ok)return {false,{},field.error};if(field.value)return {false,{},common_failure("FieldBattleMovementBlocked",op.actor_id).error};auto* actor=common.actor(op.actor_id);if(!actor)return fail<Done>("UnknownActor",ErrorKind::internal);auto id=actor->action_id;auto* road=state.road_actions.find(id);auto* terrain=state.actions.find(id);if(!road&&!terrain)return fail<Done>("InvalidTravelAction",ErrorKind::internal);bool pause=std::holds_alternative<PauseTravelCommand>(op.command)||std::holds_alternative<TravelWaitCommand>(op.command);if(pause||std::holds_alternative<ResumeTravelCommand>(op.command)){if(road){if(!road->has_paused&&!road->field_order.empty())road->field_order.push_back("paused");road->has_paused=true;road->paused=pause;}else terrain->paused=pause;return common_success();}if(!op.quote)return fail<Done>("InvalidTravelQuote",ErrorKind::internal);return road?replace_road_plan(common,state,catalog,id,*op.quote):begin_terrain_travel(common,state,id,*op.quote);}
// 原暂停/复行/改道执行前后唯一快照，TravelWait本函数不推进钟或扣费。
Status apply_travel_control(CommonWrite& common,TravelState& state,const TravelCatalog& catalog,const PreparedTravelOp& op,std::optional<TravelControlDraft>* control){
    if(!control)return apply_travel_control_without_capture(common,state,catalog,op);control->reset();
    const auto* actor=common.actor(op.actor_id);if(!actor)return common_failure("UnknownActor",{},ErrorKind::internal);const auto id=actor->action_id;
    auto before=capture_original_travel_plan_snapshot(common,state,id);if(!before.ok)return {false,{},before.error};if(!before.value.params)return common_failure("TravelControlCaptureOriginalPayloadRequired",id);
    TravelControlDraft draft;draft.source_tick=common.tick();draft.original_control=op;draft.before=std::move(before.value);
    auto applied=apply_travel_control_without_capture(common,state,catalog,op);if(!applied.ok)return applied;
    auto after=capture_original_travel_plan_snapshot(common,state,id);if(!after.ok)return {false,{},after.error};if(!after.value.params)return common_failure("TravelControlCaptureInstalledPayloadRequired",id);
    draft.after=std::move(after.value);*control=std::move(draft);return common_success();
}
// C安装encounter后调用一次，只恢复行动前截断快照，决不重复收本半日费用。
static Status apply_contact_truncation_without_capture(CommonWrite& common,TravelState& state,const ContactHit& hit,const StableId& encounter_id,bool interactive){
    if(hit.road)return apply_road_contact_truncation(common,state,hit,encounter_id,interactive);
    for(const auto* endpoint:{&hit.first,&hit.second}){const auto& trace=endpoint->trace;auto* action=common.action(trace.action_id);if(!action||action->kind!="travel"||action->actor_id!=endpoint->actor_id||!state.actions.find(trace.action_id))return fail<Done>("EncounterWithoutJourney",ErrorKind::internal);auto p=trace.params;p.paused=interactive?true:trace.params.paused;p.encounter_id=interactive?encounter_id:StableId{};if(!p.has_encounter_id&&!p.field_order.empty())p.field_order.push_back("encounter_id");if(!p.has_met_actions&&!p.field_order.empty())p.field_order.push_back("met_actions");p.has_encounter_id=p.has_met_actions=true;p.met_actions.push_back(endpoint==&hit.first?hit.second.trace.action_id:hit.first.trace.action_id);state.actions.set(trace.action_id,std::move(p));ActorMutation change;change.actor_id=endpoint->actor_id;change.location=endpoint->anchor;auto moved=common.apply_actor(change);if(!moved.ok)return moved;}return common_success();
}
// 真实半日已支付后截停，前态是未截短的实际落点，原hit另保几何起点；不重收款。
Status apply_contact_truncation(CommonWrite& common,TravelState& state,const ContactHit& hit,const StableId& encounter_id,bool interactive,std::optional<TravelContactTruncationDraft>* contact){
    if(!contact)return apply_contact_truncation_without_capture(common,state,hit,encounter_id,interactive);contact->reset();
    std::array<StableId,2> ids=hit.road?std::array<StableId,2>{hit.road->first.trace.action_id,hit.road->second.trace.action_id}:std::array<StableId,2>{hit.first.trace.action_id,hit.second.trace.action_id};
    if(encounter_id.empty()||ids[0].empty()||ids[1].empty()||ids[0]==ids[1])return common_failure("TravelContactCaptureOriginalSourceRequired",encounter_id);
    TravelContactTruncationDraft draft;draft.source_tick=common.tick();draft.original_encounter_id=encounter_id;draft.interactive=interactive;draft.original_hit=hit;
    for(std::size_t i=0;i<ids.size();++i){auto before=capture_original_travel_plan_snapshot(common,state,ids[i]);if(!before.ok)return {false,{},before.error};if(!before.value.params)return common_failure("TravelContactCapturePayloadRequired",ids[i]);draft.before[i]=std::move(before.value);}
    auto applied=apply_contact_truncation_without_capture(common,state,hit,encounter_id,interactive);if(!applied.ok)return applied;
    for(std::size_t i=0;i<ids.size();++i){auto after=capture_original_travel_plan_snapshot(common,state,ids[i]);if(!after.ok)return {false,{},after.error};if(!after.value.params)return common_failure("TravelContactCapturePayloadRequired",ids[i]);draft.after[i]=std::move(after.value);}
    *contact=std::move(draft);return common_success();
}
// 道别恢复原暂停位，不退款、不修改剩余ticks或再跑travel step。
Status resume_after_encounter(TravelState& state,const StableId& action_id,bool prior_paused){if(auto* p=state.road_actions.find(action_id)){p->encounter_id.clear();if(!p->has_encounter_id&&!p->field_order.empty())p->field_order.push_back("encounter_id");p->has_encounter_id=true;if(!p->has_paused&&!p->field_order.empty())p->field_order.push_back("paused");p->has_paused=true;p->paused=prior_paused;return common_success();}auto* p=state.actions.find(action_id);if(!p)return fail<Done>("EncounterWithoutJourney",ErrorKind::internal);p->encounter_id.clear();if(!p->has_encounter_id&&!p->field_order.empty())p->field_order.push_back("encounter_id");p->has_encounter_id=true;p->paused=prior_paused;return common_success();}
// 原载荷清掉encounter前保存它的实际ID；输出不代表Interaction已写成resolved。
Status resume_after_encounter(const CommonRead& common,TravelState& state,const StableId& action_id,bool prior_paused,std::optional<TravelEncounterResumeDraft>* resume){
    if(!resume)return resume_after_encounter(state,action_id,prior_paused);resume->reset();
    auto before=capture_original_travel_plan_snapshot(common,state,action_id);if(!before.ok)return {false,{},before.error};if(!before.value.params)return common_failure("TravelResumeCapturePayloadRequired",action_id);
    const auto encounter=std::visit([](const auto& p){return p.encounter_id;},*before.value.params);if(encounter.empty())return common_failure("TravelResumeCaptureEncounterRequired",action_id);
    TravelEncounterResumeDraft draft;draft.source_tick=common.tick();draft.original_encounter_id=encounter;draft.original_resume_paused=prior_paused;draft.before=std::move(before.value);
    auto applied=resume_after_encounter(state,action_id,prior_paused);if(!applied.ok)return applied;
    auto after=capture_original_travel_plan_snapshot(common,state,action_id);if(!after.ok)return {false,{},after.error};if(!after.value.params)return common_failure("TravelResumeCapturePayloadRequired",action_id);draft.after=std::move(after.value);*resume=std::move(draft);return common_success();
}
// 按旧排序行动快照计算普通接触；军事分流保原规则，并显式保留能力错误。
Outcome<ContactBatch> detect_travel_contacts(const CommonRead& common,const TravelState& state,const TravelCatalog& catalog,const TravelServices& services,Integer token,const std::vector<StableId>& hold){
    ContactBatch batch;batch.source_tick=common.tick();batch.phase_token=token;if(!catalog.road_encounters)return {true,std::move(batch),{}};auto ids=common.action_order();std::sort(ids.begin(),ids.end());
    for(const auto& id:ids){auto* action=common.action(id);if(!action)return fail<ContactBatch>("MissingAction",ErrorKind::internal);auto army=services.active_army(action->actor_id);if(!army.ok)return propagate<ContactBatch>(army);if(army.value)continue;if(action->kind!="travel")continue;if(state.road_actions.find(id))continue;auto* p=state.actions.find(id);if(!p)return fail<ContactBatch>("UnsupportedNativeCapability",ErrorKind::unsupported_capability);if(!p->encounter_id.empty())continue;auto motion=terrain_motion(common,catalog,services,*action,*p,hold);if(!motion.ok)return propagate<ContactBatch>(motion);batch.read_versions.insert(batch.read_versions.end(),motion.value.read_versions.begin(),motion.value.read_versions.end());batch.motions.push_back(std::move(motion.value));}
    ContactEntrances entrances;if(!catalog.world_map_id.empty())for(const auto& row:catalog.settlements.rows())entrances[catalog.world_map_id].push_back(row.second.map_xy);
    auto hits=detect_terrain_contacts(batch.motions,entrances,&batch.candidate_pairs,&batch.exact_pairs);
    for(auto& hit:hits){bool legal=true;for(const auto* endpoint:{&hit.first,&hit.second}){const auto& p=endpoint->trace.params;auto* map=catalog.maps.find(endpoint->anchor.map_id);auto m=mode(p.mode);if(!map||!m.ok)return fail<ContactBatch>("InvalidTerrainJourney",ErrorKind::internal);if(!terrain::point_valid_prepared(*map,endpoint->anchor.point,m.value,services.terrain_flags()))legal=false;if(p.points.empty()||!terrain::segment_cost_prepared(*map,endpoint->anchor.point,p.points.front(),m.value,{}).ok)legal=false;}if(legal)batch.ordered_hits.push_back(std::move(hit));}
    auto roads=detect_road_contacts(common,state,catalog,services,hold);if(!roads.ok)return propagate<ContactBatch>(roads);batch.ordered_hits.insert(batch.ordered_hits.end(),std::make_move_iterator(roads.value.begin()),std::make_move_iterator(roads.value.end()));
    // 原普通道路层使用连接串比较平局；即使ID含冒号也不能换成tuple规则。
    std::sort(batch.ordered_hits.begin(),batch.ordered_hits.end(),[](const ContactHit& a,const ContactHit& b){Integer left=a.time_num*b.time_den,right=b.time_num*a.time_den;return left==right?(a.a+":"+a.b)<(b.a+":"+b.b):left<right;});std::unordered_set<StableId> occupied;std::vector<ContactHit> selected;for(auto& hit:batch.ordered_hits){if(occupied.count(hit.a)||occupied.count(hit.b))continue;occupied.insert(hit.a);occupied.insert(hit.b);selected.push_back(std::move(hit));}batch.ordered_hits=std::move(selected);return {true,std::move(batch),{}};
}
// 资源读版本仅检测缓存失效；调用者仍必须can_step重核当前候选。
bool contact_versions_current(const CommonRead& common,const ContactBatch& batch){for(const auto& expected:batch.read_versions)if(common.read_version(expected.resource).version!=expected.version)return false;return common.tick()==batch.source_tick;}
// 原Behavior授权由services提供，同一人物排序、一次跳过当前城市和普通拒绝都保留。
Outcome<std::vector<TravelScheduleProposal>> travel_schedule_proposals(const CommonRead& common,const TravelState& state,const TravelCatalog& catalog,const TravelServices& services){
    std::vector<TravelScheduleProposal> result;auto ids=common.actor_order();std::sort(ids.begin(),ids.end());for(const auto& id:ids){const auto* plan=services.schedule(id);if(!plan||id==services.player_actor_id())continue;const auto* itinerary=state.itineraries.find(id);if(!itinerary)continue;const auto* actor=common.actor(id);if(!actor)return fail<std::vector<TravelScheduleProposal>>("UnknownActor",ErrorKind::internal);if(!actor->action_id.empty()||itinerary->ready_tick>common.tick()||!services.party_id(id).empty())continue;if(!actor->alive||!actor->available||!std::holds_alternative<SettlementLocation>(actor->location))continue;const auto& location=std::get<SettlementLocation>(actor->location);const auto* settlement=catalog.settlements.find(location.settlement_id);if(!settlement||location.place_id!=settlement->entry_place||services.talking(id))continue;Integer index=itinerary->index,count=static_cast<Integer>(plan->destinations.size());if(index<0||index>=count)return fail<std::vector<TravelScheduleProposal>>("InvalidItinerary");if(plan->destinations[static_cast<std::size_t>(index)]==location.settlement_id)index=(index+1)%count;if(plan->destinations[static_cast<std::size_t>(index)]==location.settlement_id)continue;result.push_back({id,plan->destinations[static_cast<std::size_t>(index)],(index+1)%count});}return {true,std::move(result),{}};
}
// clock之后完成抵达休整，只更新已授权真实人物已有游标。
Status finish_travel_arrivals(const CommonRead& common,TravelState& state,const TravelServices& services){auto ids=common.actor_order();std::sort(ids.begin(),ids.end());for(const auto& id:ids){const auto* plan=services.schedule(id);if(!plan)continue;auto* itinerary=state.itineraries.find(id);if(!itinerary)continue;const auto* actor=common.actor(id);if(!actor)return fail<Done>("UnknownActor",ErrorKind::internal);if(itinerary->travelling&&actor->action_id.empty()){itinerary->travelling=false;itinerary->ready_tick=common.tick()+plan->dwell_ticks;}}return common_success();}
// 仅在正式NPC travel begin成功后登记进度，普通报价拒绝保持原游标。
Status mark_travel_departure(TravelState& state,const TravelScheduleProposal& proposal){auto* itinerary=state.itineraries.find(proposal.actor_id);if(!itinerary)return fail<Done>("InvalidItinerary",ErrorKind::internal);itinerary->index=proposal.next_index;itinerary->travelling=true;return common_success();}
} // 所有对外Status都可失败，整候选回滚由唯一事务层保证。

// 行旅终态验证按旧域首错推进；临时封路不损坏存档，实际can_step仍读取当前旗标。
#include "travel.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
// 验证失败保持旧规则码，不将缺能力当普通允许。
Status fail(const StableId& code){return common_failure(code);}
// 权威位置按整数精确比较。
bool equal(PointI a,PointI b){return a.x==b.x&&a.y==b.y;}
// 目的地类型也是语义，不把城市与其坐标视为同一个保存值。
bool equal(const TravelDestination& a,const TravelDestination& b){if(a.index()!=b.index())return false;if(auto* id=std::get_if<StableId>(&a))return *id==std::get<StableId>(b);return equal(std::get<PointI>(a),std::get<PointI>(b));}
// 点数组保序和重复，不用集合比较掩盖路径后缀差异。
bool equal(const std::vector<PointI>& a,const std::vector<PointI>& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(!equal(a[i],b[i]))return false;return true;}
// 模式校验只接受旧模式表。
std::optional<terrain::Mode> mode(const StableId& s){if(s=="foot")return terrain::Mode::foot;if(s=="army")return terrain::Mode::army;if(s=="boat")return terrain::Mode::boat;return {};}
// 字段序元数据只允许本域有限schema键，不在热验证重建边界Value。
bool valid_order(const std::vector<StableId>& order,std::initializer_list<const char*> keys){if(order.empty())return true;if(order.size()!=keys.size())return false;for(const auto* key:keys)if(std::count(order.begin(),order.end(),key)!=1)return false;return true;}
// 两个旧可选路遇字段保真实存在性；其余14个字段仍为本profile必需。
bool valid_param_order(const TerrainTravelParams& p){auto order=p.field_order;if(order.empty())return true;if(p.has_encounter_id){auto i=std::find(order.begin(),order.end(),"encounter_id");if(i==order.end())return false;order.erase(i);}if(p.has_met_actions){auto i=std::find(order.begin(),order.end(),"met_actions");if(i==order.end())return false;order.erase(i);}return valid_order(order,{"terrain","plan_id","map_id","route_index","points","destination","origin","mode","budget","travel_credit","payer_wallet_id","money_per_tick","fatigue_per_tick","paused"});}
// 道路可选键按各自存在位验证；删除或伪造顺序都不能掩盖持久字段差异。
bool valid_param_order(const RoadTravelParams& p){
    auto order=p.field_order;if(order.empty())return true;
    for(const auto& optional:std::initializer_list<std::pair<const char*,bool>>{{"destination",p.destination.has_value()},{"via",p.via.has_value()},{"terrain",p.has_terrain},{"paused",p.has_paused},{"reverse_first",p.has_reverse_first},{"encounter_id",p.has_encounter_id},{"met_actions",p.has_met_actions}}){
        if(!optional.second)continue;auto i=std::find(order.begin(),order.end(),optional.first);if(i==order.end())return false;order.erase(i);
    }
    return valid_order(order,{"path","edge_index","returning","payer_wallet_id"});
}
// 道路验证沿GD的钱包、方向、连续路径和当前位置首错顺序，不因临时封路拒绝读档。
Status validate_action(const CommonRead& common,const TravelCatalog& catalog,const ActionCoreView& action,const RoadTravelParams& p){
    const auto* actor=common.actor(action.actor_id);
    if(p.path.empty()||p.edge_index<0||p.edge_index>=static_cast<Integer>(p.path.size())||!actor||p.payer_wallet_id!=actor->wallet_id)return fail("InvalidTravelAction");
    if((!p.has_paused&&p.paused)||(!p.has_reverse_first&&p.reverse_first))return fail("InvalidJourneyControl");
    if(p.reverse_first&&p.returning!=(p.edge_index==0))return fail("InvalidReturnDirection");
    StableId previous;std::size_t leg_index=0;
    for(const auto& id:p.path){
        const auto* edge=catalog.roads.find(id);if(!edge)return fail("InvalidTravelPath");
        if(!previous.empty()&&previous!=edge->origin)return fail("DisconnectedTravelPath");
        previous=leg_index==0&&p.reverse_first?edge->origin:edge->destination;++leg_index;
    }
    const auto* location=std::get_if<OnRouteLocation>(&actor->location);
    if(!location||location->edge_id!=p.path[static_cast<std::size_t>(p.edge_index)])return fail("RouteActionMismatch");
    return common_success();
}
// 会面ID类型由codec保证；缺失位不得隐藏值，已会面列表只检查旧前缀而不排序或去重。
template<class Params> Status validate_encounter_fields(const Params& p){
    if(!p.has_encounter_id&&!p.encounter_id.empty())return fail("InvalidEncounterReference");
    if((!p.has_met_actions&&!p.met_actions.empty())||p.met_actions.size()>4096)return fail("InvalidMetJourneys");
    for(const auto& id:p.met_actions)if(id.rfind("action:",0)!=0)return fail("InvalidMetJourneys");
    return common_success();
}
// 活动行旅完整后缀、冻结预算和原路线均须合法，不能因当前封路强制修复。
Status validate_action(const CommonRead& common,const TravelState& state,const TravelCatalog& catalog,const TravelServices& services,const ActionCoreView& action,const TerrainTravelParams& p){
    auto* map=catalog.maps.find(p.map_id);auto m=mode(p.mode);if(!map||p.map_id!=catalog.world_map_id||!catalog.settlements.find(p.origin)||!m)return fail("InvalidTerrainJourney");
    if(p.budget<1||p.money_per_tick<0||p.fatigue_per_tick<0||p.points.empty())return fail("InvalidTerrainJourney");
    auto* actor=common.actor(action.actor_id);if(!actor)return fail("InvalidTerrainJourneyActor");Outcome<TravelProfile> profile;if(p.mode=="boat"){auto frozen=services.vessel_journey(actor->id,action.id,p.plan_id);if(!frozen.ok)return {false,{},frozen.error};if(!frozen.value||frozen.value->fleet_id.empty()||frozen.value->army_id.empty()||frozen.value->commander_actor_id!=actor->id||frozen.value->travel_budget<1)return fail("NavalJourneySourceRequired");profile={true,{"boat",frozen.value->payer_wallet_id,0,frozen.value->travel_budget},{}};}else profile=travel_profile(common,services,actor->id);auto* location=std::get_if<WildernessLocation>(&actor->location);if(!profile.ok||p.mode!=profile.value.mode||p.payer_wallet_id!=profile.value.payer_wallet_id||!location||location->map_id!=p.map_id)return fail("InvalidTerrainJourneyActor");
    auto* fact=state.journey_facts.find(p.plan_id);if(!fact||fact->actor_id!=actor->id||fact->action_id!=action.id||fact->map_id!=p.map_id||fact->budget!=p.budget||fact->mode!=p.mode||!equal(fact->destination,p.destination))return fail("TerrainJourneyFactMismatch");
    if(fact->tick<action.start_tick||fact->tick>common.tick()||fact->points.size()<2||!equal(fact->origin,fact->points.front()))return fail("TerrainJourneyFactMismatch");
    if(p.route_index<1||p.route_index>=static_cast<Integer>(fact->points.size()))return fail("TerrainJourneySuffixMismatch");
    std::vector<PointI> suffix(fact->points.begin()+p.route_index,fact->points.end());if(!equal(p.points,suffix))return fail("TerrainJourneySuffixMismatch");
    PointI target;if(auto* id=std::get_if<StableId>(&p.destination)){auto* settlement=catalog.settlements.find(*id);if(!settlement)return fail("TerrainJourneyTargetMismatch");target=settlement->map_xy;}else target=std::get<PointI>(p.destination);if(!equal(p.points.back(),target))return fail("TerrainJourneyTargetMismatch");
    if(p.travel_credit<0||p.travel_credit>terrain::MAX_COST)return fail("InvalidTerrainCredit");
    Integer maximum=0;for(const auto& budget:map->budgets)maximum=std::max(maximum,budget.second);Integer elapsed=std::max<Integer>(0,common.tick()-action.start_tick);if(p.travel_credit>std::min<Integer>(terrain::MAX_COST,elapsed*maximum))return fail("InvalidTerrainCredit");
    if(p.money_per_tick!=map->money_per_tick+services.party_extra_cost(actor->id)||p.fatigue_per_tick!=map->fatigue_per_tick)return fail("TerrainJourneyCostMismatch");
    auto budget=map->budgets.find(*m);if(fact->starting_load<0||budget==map->budgets.end()||(p.mode=="boat"?(fact->starting_load!=0||profile.value.naval_budget<1||p.budget!=std::min(budget->second,profile.value.naval_budget)):p.budget!=std::max<Integer>(1,budget->second*100/(100+fact->starting_load*4))))return fail("TerrainJourneyBudgetMismatch");
    PointI here=fact->points.front();for(std::size_t i=1;i<fact->points.size();++i){if(!terrain::segment_cost_prepared(*map,here,fact->points[i],*m,{}).ok)return fail("TerrainJourneyFactMismatch");here=fact->points[i];}
    here=location->point;for(auto next:p.points){if(!terrain::segment_cost_prepared(*map,here,next,*m,{}).ok)return fail("InvalidTerrainJourneyPath");here=next;}return common_success();
}
} // 只校验B域，encounter/军令引用由C owner在同一终态继续核对。

// 主行动顺序来自唯一共同表；本域孤立payload和未授权NPC行程均明确失败。
Status validate_travel_domain(const CommonRead& common,const TravelState& state,const TravelCatalog& catalog,const TravelServices& services){
    // 无行动停驻者也核基础地形；只有具名实船占用走boat几何，船损不授出航权也不使合法浮泊成为坏档。
    for(const auto& id:common.actor_order()){const auto* actor=common.actor(id);if(!actor)return fail("InvalidActor");if(const auto* p=std::get_if<WildernessLocation>(&actor->location)){const auto* map=catalog.maps.find(p->map_id);if(!map||p->map_id!=catalog.world_map_id||!catalog.settlements.find(p->origin))return fail("InvalidWildernessLocation");auto occupied=services.vessel_location(id);if(!occupied.ok)return {false,{},occupied.error};auto geometry=terrain::Mode::foot;if(occupied.value){const auto& source=*occupied.value;if(source.commander_actor_id!=id||source.fleet_id.empty()||source.army_id.empty()||source.formation_source_id.empty()||source.world_map_id!=p->map_id)return fail("NavalLocationSourceRequired");geometry=terrain::Mode::boat;}else if(!actor->alive){auto remains=services.vessel_remains(id);if(!remains.ok)return {false,{},remains.error};if(remains.value){const auto& proof=*remains.value;if(proof.actor_id!=id||proof.fleet_id.empty()||proof.army_id.empty()||proof.round_id.empty()||proof.casualty_id.empty()||actor->available||actor->hp!=0||!actor->action_id.empty()||proof.anchor.map_id!=p->map_id||proof.anchor.origin!=p->origin||proof.anchor.point.x!=p->point.x||proof.anchor.point.y!=p->point.y)return fail("NavalRemainsLocationSourceRequired");geometry=terrain::Mode::boat;}}if(!terrain::point_valid_prepared(*map,p->point,geometry,{}))return fail("InvalidWildernessPoint");}else if(const auto* p=std::get_if<OnRouteLocation>(&actor->location)){const auto* road=catalog.roads.find(p->edge_id);if(!road||p->progress<0||road->duration_ticks<1||road->duration_ticks>720||p->progress>road->duration_ticks*100||p->origin!=road->origin)return fail("InvalidRouteLocation");}}

    for(const auto& row:state.actions.rows())if(!valid_param_order(row.second))return fail("InvalidTravelFieldOrder");
    for(const auto& row:state.road_actions.rows())if(!valid_param_order(row.second))return fail("InvalidTravelFieldOrder");
    for(const auto& row:state.journey_facts.rows())if(row.second.id!=row.first)return fail("TerrainJourneyFactMismatch");
    for(const auto& row:state.journey_facts.rows())if(!valid_order(row.second.field_order,{"kind","actor_id","action_id","tick","map_id","origin","destination","mode","budget","starting_load","points"}))return fail("InvalidTravelFieldOrder");
    for(const auto& row:state.itineraries.rows())if(!valid_order(row.second.field_order,{"index","ready_tick","travelling"}))return fail("InvalidTravelFieldOrder");
    for(const auto& id:common.action_order()){const auto* action=common.action(id);if(!action)return fail("InvalidAction");if(action->kind=="travel"){const auto* terrain=state.actions.find(id);const auto* road=state.road_actions.find(id);if((terrain!=nullptr)==(road!=nullptr))return fail("InvalidTravelAction");auto valid=terrain?validate_action(common,state,catalog,services,*action,*terrain):validate_action(common,catalog,*action,*road);if(!valid.ok)return valid;}else if(action->kind=="rescue"){auto* p=state.rescues.find(id);auto* actor=common.actor(action->actor_id);if(!p||!catalog.settlements.find(p->origin)||action->remaining_ticks>2||!actor)return fail("InvalidRescueAction");StableId origin;if(auto* wild=std::get_if<WildernessLocation>(&actor->location))origin=wild->origin;else if(auto* road=std::get_if<OnRouteLocation>(&actor->location))origin=road->origin;if(origin!=p->origin)return fail("InvalidRescueAction");}}
    for(const auto& row:state.actions.rows()){auto valid=validate_encounter_fields(row.second);if(!valid.ok)return valid;auto* action=common.action(row.first);if(!action||action->kind!="travel")return fail("DanglingTerrainJourney");}
    for(const auto& row:state.road_actions.rows()){auto valid=validate_encounter_fields(row.second);if(!valid.ok)return valid;auto* action=common.action(row.first);if(!action||action->kind!="travel")return fail("InvalidTravelAction");}
    for(const auto& row:state.rescues.rows()){auto* action=common.action(row.first);if(!action||action->kind!="rescue")return fail("InvalidRescueAction");}
    std::size_t expected=0;auto ids=common.actor_order();std::sort(ids.begin(),ids.end());for(const auto& id:ids){auto* plan=services.schedule(id);if(!plan)continue;++expected;auto* i=state.itineraries.find(id);if(!i||i->index<0||i->index>=static_cast<Integer>(plan->destinations.size())||i->ready_tick<0||i->ready_tick>2147483647)return fail("InvalidItinerary");}if(state.itineraries.size()!=expected)return fail("UnexpectedItinerary");return common_success();
}
} // 禁止在每个中间mutator之后调用终态全验；由事务发布前统一协调。

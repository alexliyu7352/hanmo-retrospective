#include "resident/domains/estate/estate_detachment_release.hpp"
#include "military_siege_engineering_integration.hpp"
#include "resident/domains/military/military_field_command_profile.hpp"
#include "resident/domains/military/military_field_command_rules.hpp"
// 军旅接缝只借当前候选的共同、庄园和行旅权威；不复制世界、钱粮或随机流。
#include "political_takeover_execution_world.hpp"
#include "world_bundle.hpp"
#include "native_military_state.hpp"
#include "native_military_battle_lifecycle.hpp"
#include "resident/storage/common_codec.hpp"
#include "military_tactic_learning_integration.hpp"
#include "resident/domains/military/military_officer_source.hpp"
#include "resident/domains/military/military_roster_history.hpp"
#include "resident/domains/military/naval_custody.hpp"
#include "resident/domains/affairs/affairs_supply_suspension.hpp"
#include <algorithm>
#include <stdexcept>

namespace hanmo::resident {
// 回营选址只比较真实军团地形成本，不提前检查人物忙碌、城门位置或钱包。
Outcome<Integer> military_return_route(const WorldBundle& world,const StableId& army_id,const StableId& destination_settlement_id) {
    const auto& catalog=*world.catalog->travel;
    const ArmyRecord* army=nullptr;
    for(const auto& candidate:world.military.armies)if(candidate.id==army_id){army=&candidate;break;}
    if(!army)return {false,{},common_failure("UnknownArmy",army_id).error};
    const auto* destination=catalog.settlements.find(destination_settlement_id);
    if(!destination)return {false,{},common_failure("UnknownSettlement",destination_settlement_id).error};
    if(world.services.army_embarked(army_id)&&!world.services.naval_berth(destination->entry_place))return {false,{},common_failure("NavalDestinationBerthRequired",destination_settlement_id).error};
    if(army_at_settlement(world.common,world.military,catalog,army_id,destination_settlement_id))return {true,0,{}};
    const auto* map=catalog.maps.find(catalog.world_map_id);
    const auto* commander=world.common.actor(army->commander_actor_id);
    // 原军令回营在无地形地图时比较道路时耗；没有道路或在路上的旧领域分支只留请求边界。
    if(!map){
        if(!catalog.roads.empty()&&commander)if(const auto* city=std::get_if<SettlementLocation>(&commander->location)){
            auto route=plan_road_route(catalog,city->settlement_id,TravelDestination{destination_settlement_id},world.common.tick());
            if(!route.ok)return {false,{},route.error};return {true,route.value.duration,{}};
        }
        return {true,0,{}};
    }
    std::optional<PointI> origin;
    if(commander) {
        if(const auto* wilderness=std::get_if<WildernessLocation>(&commander->location)) {
            if(wilderness->map_id==catalog.world_map_id)origin=wilderness->point;
        } else if(const auto* city=std::get_if<SettlementLocation>(&commander->location)) {
            if(const auto* settlement=catalog.settlements.find(city->settlement_id))origin=settlement->map_xy;
        }
    }
    if(!origin)return {false,{},common_failure("CampaignCommanderLocationRequired").error};
    const auto& flags=world.services.terrain_flags();
    const auto mode=world.services.army_embarked(army_id)?terrain::Mode::boat:terrain::Mode::army;
    auto route=catalog.route_cache?catalog.route_cache->plan(*map,*origin,destination->map_xy,mode,flags):terrain::plan_prepared(*map,*origin,destination->map_xy,mode,flags);
    if(!route.ok)return {false,{},route.error};
    return {true,route.value.cost,{}};
}

// 每次重建有限服务闭包；所有可变读取在调用时取当前Bundle，不能跨候选保存。
MilitaryAccess BundleServices::military_access(std::optional<Integer> population_phase_tick,const DynamicStepInput* occupation_phase) const {
    MilitaryAccess access;
    auto selected=selected_native_military_state_versions(world_.catalog->identity);
    if(!selected.ok)throw boundary::DecodeFailure{selected.error};
    // Selected exact authority never silently uses the legacy tick-only path.
    // The complete owning composition must supply the genuine source context.
    access.military_authority_time_schema=selected.value.military_authority_time;
    access.military_action_resources_schema=selected.value.military_action_resources;
    access.military_return_detachments_schema=selected.value.military_return_detachments;
    access.military_action_conditions_schema=selected.value.military_action_conditions;
    access.military_training_completion_time_schema=selected.value.military_training_completion_time;
    access.military_formation_reservations_schema=selected.value.military_formation_reservations;
    auto lifecycle=selected_native_military_battle_lifecycle(world_.catalog->identity);if(!lifecycle.ok)throw boundary::DecodeFailure{lifecycle.error};
    access.military_battle_lifecycle_schema=lifecycle.value;
    // The actual apply or exact lease supplies the context, never this factory's stack.

    access.occupation_source_schema=world_.catalog->selected_military_occupation_schema;
    if(access.occupation_source_schema==2){access.occupation_clock=&occupation_clock_;access.occupation_common=&world_.common;}
    if(world_.military_siege_engineering_enabled())access.siege_engineering=make_military_siege_engineering_access(world_);
    if(world_.military_tactic_learning_enabled()){access.tactic_learning_state=&world_.military_tactic_learning;access.preview_tactic_practice=[this](const StableId& actor,const MilitaryTacticPracticeClaim& request){return preview_military_tactic_practice_world(world_,actor,request);};access.apply_tactic_practice=[this](const StableId& actor,const MilitaryTacticPracticeClaim& request){return apply_military_tactic_practice_world_in_candidate(world_,actor,request);};}
    if(world_.political_military_appointments_enabled())access.officer_appointments=&military_appointments_;
    if(world_.political_field_appointments_enabled())access.field_appointments=&field_appointments_;
    access.naval_state=world_.naval_enabled()?&world_.naval:nullptr;
    access.army_command_before=[this](const StableId& army,const StableId& fact){return query_military_army_command_source_before(world_.common,world_.military,army,fact);};
    access.field_command_source=[this](MilitaryFieldCommandKind kind,const StableId& army,const StableId& unit){return query_military_field_command_source(world_.common,world_.military,*world_.catalog->military,world_.naval_enabled()?&world_.naval:nullptr,kind,army,unit);};
    access.fleet_command_source=[this](const StableId& fleet)->Outcome<MilitaryArmyCommandBinding>{for(const auto& f:world_.naval.fleets)if(f.id==fleet)return query_military_fleet_command_source(world_.common,world_.military,f);return {false,{},common_failure("MilitaryFieldFleetSourceRequired",fleet).error};};
    access.field_actor_occupation_clear=[this](const StableId& actor){return validate_military_field_actor_occupation(world_.common,world_.military,world_.naval_enabled()?&world_.naval:nullptr,actor);};
    access.apply_field_fleet_binding=[this](const MilitaryFieldCommandFact& f){return apply_military_field_fleet_binding(world_.common,world_.military,world_.naval,f);};
    access.unit_command_source=[this](const StableId& unit){return query_military_unit_command_source(world_.common,world_.military,*world_.catalog->military,unit);};
    access.actual_catalog_identity=&world_.catalog->identity;
    access.common_catalog=world_.catalog->common.get();
    access.travel_catalog=world_.catalog->travel.get();
    access.travel_state=&world_.travel;
    access.travel_services=this;
    // 冷读只借原稳定Common事实边界，调用方不能借未来tick补全军官来源。
    access.rosters_before=[this](const std::vector<StableId>& units,Integer tick,const StableId& fact){return military_rosters_before(world_.common,world_.military,*world_.catalog->military,units,tick,fact,world_.population_military_enabled()?&world_.population_military:nullptr,world_.population_military_enabled()?world_.catalog->population_military.get():nullptr);};
    access.end_journey=[this](const StableId& id){const auto* action=world_.common.action(id);if(!action||action->kind!="travel"||(!world_.travel.actions.find(id)&&!world_.travel.road_actions.find(id)))return common_failure("MilitaryJourneyTerminationSourceRequired",id);return release_action(world_,id);};
    access.army_embarked=[this](const StableId& army){return army_embarked(army);};
    access.interaction=&world_.interaction;
    access.interaction_catalog=world_.catalog->interaction.get();
    // 交互条件由中央调用点提供同栈借用，不能保存临时InteractionAccess的地址。
    access.interaction_access=nullptr;
    if(world_.population_military_enabled()){
        access.population_roster=[this,population_phase_tick](const StableId& source,const StableId& unit){auto phase=population_phase_tick&&*population_phase_tick>world_.common.tick()?population_phase_tick:std::optional<Integer>{};return population_military_roster(world_.population_read(phase),source,unit);};
        access.population_historical_token=[this,population_phase_tick](const StableId& member,Integer tick){auto phase=population_phase_tick&&*population_phase_tick>world_.common.tick()?population_phase_tick:std::optional<Integer>{};return population_military_historical_member(world_.population_read(phase),member,tick);};
        access.population_training_member=[this,population_phase_tick](const StableId& source,const StableId& unit,const StableId& member,Integer tick){auto phase=population_phase_tick&&*population_phase_tick>world_.common.tick()?population_phase_tick:std::optional<Integer>{};return population_military_training_member(world_.population_read(phase),source,unit,member,tick);};
    }
    if(world_.naval_combat_enabled()){access.naval_custodian=[this](const StableId& actor){return naval_custodian(world_.naval,actor);};access.naval_defeat_source=[this](const StableId& army){return naval_defeat_source(world_.naval,world_.military,army);};}
    access.player_actor_id=world_.common.state().metadata.player_actor_id;
    const auto* battle=world_.common.state().rng.find("battle");
    // 缺流时此标量不能用于开战；战斗入口通过rng_stream返回有限失败。
    access.battle_seed=battle?battle->seed:0;
    access.in_open_battle=[this](const StableId& actor){return in_open_battle(actor);};
    access.return_route_cost=[this](const StableId& army,const StableId& destination){return military_return_route(world_,army,destination);};
    access.preview_travel=[this](const StableId& actor,const StableId& destination)->Status {
        auto quote=plan_travel(world_.common,world_.travel,*world_.catalog->travel,*this,actor,TravelDestination{destination});
        return quote.ok?common_success():Status{false,{},quote.error};
    };
    access.resolve_location=[this](const StableId& actor) {
        const auto location=resolved_location(actor);
        return location.ok?location.value:Location{SettlementLocation{}};
    };
    access.in_party=[this](const StableId& actor){return !party_id(actor).empty();};
    access.item_weight=[this](const StableId& item) {
        const auto* definition=world_.catalog->common->items.find(item);
        if(!definition)throw std::logic_error("InvalidResidentCatalog: military item "+item);
        return definition->weight;
    };
    access.actor_injury=[this](const StableId& actor) {
        const auto* person=world_.common.state().actors.find(actor);
        return person?person->injury:Integer{3};
    };
    access.actor_skill=[this](const StableId& actor,const StableId& skill)->std::optional<Integer> {
        const auto* person=world_.common.state().actors.find(actor);
        if(!person)return std::nullopt;
        const auto value=growth_skill_xp(person->skills,skill);
        return value<0?std::nullopt:std::optional<Integer>{value};
    };
    access.actor_attribute=[this](const StableId& actor,const StableId& attribute)->std::optional<Integer> {
        const auto* person=world_.common.state().actors.find(actor);
        if(!person)return std::nullopt;
        const auto& values=person->attributes;
        if(attribute=="administration")return values.administration;
        if(attribute=="charisma")return values.charisma;
        if(attribute=="intellect")return values.intellect;
        if(attribute=="leadership")return values.leadership;
        if(attribute=="martial")return values.martial;
        return std::nullopt;
    };
    access.rng_stream=[this](const StableId& stream)->std::optional<RngStream> {
        const auto* value=world_.common.state().rng.find(stream);
        return value?std::optional<RngStream>{*value}:std::nullopt;
    };
    access.next_instance_id=[this](){return world_.common.state().next_instance_id;};
    access.fact_exists=[this](const StableId& id){return world_.common.state().facts.find(id)!=nullptr;};
    access.retainer_source=[this](const StableId& actor,const StableId& order)->std::optional<RetainerRecord> {
        for(const auto& row:world_.estate.estates.rows()) {
            const auto* source=row.second.detached.find(actor);
            const auto* header=source?world_.common.state().facts.find(*source):nullptr;
            if(!header||header->kind!="military_muster")continue;
            for(const auto& fact:world_.military.muster_facts) {
                if(fact.id!=*source||fact.estate_id!=row.first||fact.order_id!=order||
                    std::find(fact.actor_ids.begin(),fact.actor_ids.end(),actor)==fact.actor_ids.end())continue;
                // 军事报价继续用所有历史army.retainers拒绝重复借据，并保留原专属错误码。
                return RetainerRecord{actor,row.first,*source,{},{}};
            }
        }
        return std::nullopt;
    };
    access.preview_estate_muster=[this](const StableId& actor,const StableId& estate,const std::vector<StableId>& people) {
        return hanmo::resident::preview_estate_muster(world_.common,world_.estate,*world_.catalog->estate,*world_.catalog->common,*this,actor,estate,people);
    };
    access.apply_estate_muster=[this](const StableId& actor,const StableId& estate,const std::vector<StableId>& people,const StableId& source)->Status {
        auto estate_services=estate_access();
        EstateDetachment command{estate,people,source,false,{"estate_id","actor_ids","source_id"}};
        auto quote=preview_estate(world_.common,world_.estate,*world_.catalog->estate,*world_.catalog->common,*this,actor,command,&estate_services);
        if(!quote.ok)return {false,{},quote.error};
        auto applied=apply_estate(world_.common,world_.estate,*world_.catalog->estate,*world_.catalog->common,*this,actor,quote.value,&estate_services);
        return applied.ok?common_success():Status{false,{},applied.error};
    };
    access.release_lost_retainer=[this](const StableId& estate_id,const StableId& actor,const StableId& source,OriginalEstateDetachmentReleaseSink* observation) {
        return release_original_lost_estate_retainer_in_candidate(world_.estate,estate_id,actor,source,observation);
    };
    if(world_.political_takeover_enabled()){
        access.occupation_application_required=true;
        access.apply_occupation=[this,population_phase_tick,occupation_phase](const StableId& id)->Outcome<OccupationEffect>{
            auto ia=interaction_access();auto original=military_access(population_phase_tick,occupation_phase);original.interaction_access=&ia;original.apply_occupation={};
            auto applied=apply_political_takeover_world_in_candidate(world_,*world_.catalog->political_takeover,world_.political_takeover,original,id,[this,occupation_phase](const auto&,const auto&)->Status{
                if(!world_.political_takeover_seed_complete||!world_.economic_takeover_seed_complete||!world_.tasks.takeover_execution||!world_.catalog->task_takeover_execution_enabled)return common_failure("MissingPoliticalTakeoverExecutionState");
                // 首次接管也立即收紧已有Offer；只读同候选真实阶段，不新建经济责任或提前结合同。
                auto current=PoliticalTakeoverExecutionWorldAccess::create(world_,occupation_phase);if(!current.ok)return {false,{},current.error};
                auto suspended=current.value->supply_suspensions();if(!suspended.ok)return {false,{},suspended.error};
                if(!world_.catalog->affairs)return common_failure("MissingPoliticalTakeoverWorldCatalog");
                auto projected=project_affairs_supply_suspensions_in_candidate(world_.common,world_.affairs,*world_.catalog->affairs,world_.tasks,suspended.value);if(!projected.ok)return projected;return project_economic_supply_world_in_candidate(world_,occupation_phase);
            },occupation_phase);if(!applied.ok)return {false,{},applied.error};return {true,std::move(applied.value.occupation_effect),{}};
        };
    }
    return access;
}
} // 这里只绑定跨域来源；事务提交、阶段顺序和军旅规则仍由各自唯一所有者执行。

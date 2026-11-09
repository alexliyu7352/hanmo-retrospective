// 不保存第二份责任，不在查询或验档时生成待办；回调只消费原军事本次实际apply。
#include "political_takeover_integration.hpp"
#include "world_bundle.hpp"
#include "political_takeover_reservations_world.hpp"
#include "political_takeover_execution_world.hpp"
#include "economic_supply_policy_integration.hpp"
#include "resident/domains/affairs/affairs_supply_suspension.hpp"
namespace hanmo::resident {
namespace {
Status available(const WorldBundle& w,const PoliticalTakeoverCatalog& catalog){
    if(!catalog.enabled||catalog.version!=1||!w.dynamic_enabled()||!w.politics_enabled())return common_failure("PoliticalTakeoverNotEnabled");
    if(!w.catalog->common||!w.catalog->military||!w.catalog->military->military||!w.catalog->organization||!w.catalog->organization->enabled||!w.catalog->local_economy||!w.catalog->tasks||!w.catalog->affairs)return common_failure("MissingPoliticalTakeoverWorldCatalog");
    const bool supply=w.catalog->economic_supply_policies_enabled;if(supply!=bool(w.catalog->economic_supply_policies)||(supply&&(w.political_supply_adoptions.version!=1||w.economic_supply_policies.version!=1)))return common_failure("MissingSupplyPolicyPair");
    return common_success();
}
PoliticalTakeoverOriginalContext original(const WorldBundle& w,const MilitaryAccess& military,const DynamicStepInput* phase=nullptr,const EstateEmploymentContext* employment=nullptr,const PrivateShopResponsibilitySourceContext* shops=nullptr){
    return {w.common,w.military,*w.catalog->military,military,w.local_economy,*w.catalog->local_economy,
        w.fiscal_settlement_enabled()?&w.fiscal_settlement:nullptr,
        w.fiscal_settlement_enabled()?w.catalog->fiscal_settlement.get():nullptr,
        w.public_relief_enabled()?&w.public_relief:nullptr,w.tasks,*w.catalog->tasks,
        w.governance_tasks_enabled()?&w.governance_tasks:nullptr,
        w.envoy_tasks_enabled()?&w.envoy_tasks:nullptr,phase,w.occupation_fiscal_enabled()?&w.occupation_fiscal_settlement:nullptr,employment,shops};
}
}
PoliticalTakeoverWorldAccess::PoliticalTakeoverWorldAccess(const WorldBundle& w,const PoliticalTakeoverCatalog& catalog,const PoliticalTakeoverState& state,const DynamicStepInput* phase):
    military_access_(w.services.military_access(phase?std::optional<Integer>{phase->to_tick}:std::nullopt,phase)),employment_(w.estate_employment_enabled()?std::make_unique<EstateEmploymentWorldAccess>(w,phase):nullptr),shop_sources_(w.private_shops_enabled()?std::optional<PrivateShopResponsibilitySourceContext>{PrivateShopResponsibilitySourceContext{w.common,w.local_economy,*w.catalog->local_economy,w.private_shops,*w.catalog->private_shops,phase,w.catalog->private_shop_daily_rent.get(),w.private_shop_daily_rent_enabled()?&w.private_shop_daily_rent:nullptr}}:std::nullopt),originals_(original(w,military_access_,phase,employment_?&employment_->context():nullptr,shop_sources_?&*shop_sources_:nullptr)),
    supply_pending_(w.economic_supply_policies_enabled()?std::make_unique<EconomicSupplyPendingAccess>(EconomicSupplyPendingContext{w.common,*w.catalog->common,w.local_economy,*w.catalog->local_economy,*w.catalog->economic_supply_policies,w.economic_supply_policies,phase}):nullptr),
    context_{w.common,*w.catalog->common,w.organization,*w.catalog->organization,*w.catalog->politics,catalog,state,w.military,*w.catalog->military,military_access_,originals_,phase,w.economic_supply_policies_enabled()?&w.political_supply_adoptions:nullptr,supply_pending_.get()},
    responsibilities_(context_),territories_(context_),fiscal_(context_){}
Outcome<std::unique_ptr<PoliticalTakeoverWorldAccess>> PoliticalTakeoverWorldAccess::create(const WorldBundle& w,const PoliticalTakeoverCatalog& catalog,const PoliticalTakeoverState& state,const DynamicStepInput* phase){
    auto evaluation=political_takeover_evaluation_tick(w.common,phase);if(!evaluation.ok)return {false,{},evaluation.error};
    auto ready=available(w,catalog);if(!ready.ok)return {false,{},ready.error};
    if(state.version!=1)return {false,{},common_failure("UnsupportedPoliticalTakeoverVersion").error};
    return {true,std::unique_ptr<PoliticalTakeoverWorldAccess>(new PoliticalTakeoverWorldAccess(w,catalog,state,phase)),{}};
}
Outcome<PoliticalTakeoverQuote> prepare_political_takeover_world_command(const WorldBundle& w,const PoliticalTakeoverCatalog& catalog,const PoliticalTakeoverState& state,const StableId& actor,const PoliticalTakeoverDecisionCommand& command,const WorldIdentity& identity){
    auto read=PoliticalTakeoverWorldAccess::create(w,catalog,state);if(!read.ok)return {false,{},read.error};
    return prepare_political_takeover_command(read.value->context(),actor,command,identity);
}
Outcome<PoliticalTakeoverQuote> prepare_political_takeover_world_command(const WorldBundle& w,const PoliticalTakeoverCatalog& catalog,const PoliticalTakeoverState& state,const StableId& actor,const BoundaryValue& params,const WorldIdentity& identity){
    auto read=PoliticalTakeoverWorldAccess::create(w,catalog,state);if(!read.ok)return {false,{},read.error};
    return prepare_political_takeover_command(read.value->context(),actor,params,identity);
}
Outcome<StableId> commit_political_takeover_world_command(WorldBundle& w,const PoliticalTakeoverCatalog& catalog,PoliticalTakeoverState& state,const StableId& actor,const PoliticalTakeoverQuote& quote){
    if(&state!=&w.political_takeover)return {false,{},common_failure("PoliticalTakeoverCandidateMismatch").error};
    auto read=PoliticalTakeoverWorldAccess::create(w,catalog,state);if(!read.ok)return {false,{},read.error};
    auto committed=commit_political_takeover_command_in_transaction(w.common,state,read.value->context(),actor,quote,w.economic_supply_policies_enabled()?&w.political_supply_adoptions:nullptr);if(!committed.ok)return committed;
    // 改判和本源暂停投影同属原命令候选；不结算合同、不发新Need或事实，失败交外层整候选撤回。
    auto current=PoliticalTakeoverExecutionWorldAccess::create(w);if(!current.ok)return {false,{},current.error};
    auto suspended=current.value->supply_suspensions();if(!suspended.ok)return {false,{},suspended.error};
    auto projected=project_affairs_supply_suspensions_in_candidate(w.common,w.affairs,*w.catalog->affairs,w.tasks,suspended.value);if(!projected.ok)return {false,{},projected.error};
    if(w.economic_supply_policies_enabled()){auto projection=project_economic_supply_world_in_candidate(w);if(!projection.ok)return {false,{},projection.error};}
    return committed;
}
Outcome<BoundaryValue> query_political_takeover_world(const WorldBundle& w,const PoliticalTakeoverCatalog& catalog,const PoliticalTakeoverState& state,const StableId& observer,const StableId& place){
    auto read=PoliticalTakeoverWorldAccess::create(w,catalog,state);if(!read.ok)return {false,{},read.error};
    return query_political_takeover_responsibilities(read.value->context(),observer,place);
}
Outcome<PoliticalTakeoverApplication> apply_political_takeover_world_in_candidate(WorldBundle& w,const PoliticalTakeoverCatalog& catalog,PoliticalTakeoverState& state,const MilitaryAccess& military,const StableId& occupation_id,const PoliticalTakeoverPendingApplication& execute,const DynamicStepInput* phase){
    auto evaluation=political_takeover_evaluation_tick(w.common,phase);if(!evaluation.ok)return {false,{},evaluation.error};
    auto ready=available(w,catalog);if(!ready.ok)return {false,{},ready.error};
    std::optional<EstateEmploymentWorldAccess> employment;if(w.estate_employment_enabled())employment.emplace(w,phase);
    std::optional<PrivateShopResponsibilitySourceContext> shops;if(w.private_shops_enabled())shops.emplace(PrivateShopResponsibilitySourceContext{w.common,w.local_economy,*w.catalog->local_economy,w.private_shops,*w.catalog->private_shops,phase,w.catalog->private_shop_daily_rent.get(),w.private_shop_daily_rent_enabled()?&w.private_shop_daily_rent:nullptr});
    PoliticalTakeoverApplicationContext context{original(w,military,phase,employment?&employment->context():nullptr,shops?&*shops:nullptr),*w.catalog->common,*w.catalog->organization,*w.catalog->politics,catalog,w.politics,political_takeover_reservation_read(w,phase)};
    return apply_political_military_takeover_in_transaction(w.common,w.military,state,context,occupation_id,execute);
}
}

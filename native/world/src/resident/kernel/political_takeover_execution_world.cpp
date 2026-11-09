// 同一临时对象保证政治读、垫款读和经济上下文的引用生命期覆盖实际调用。
#include "political_takeover_execution_world.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
PoliticalTakeoverExecutionWorldAccess::PoliticalTakeoverExecutionWorldAccess(const WorldBundle& w,std::unique_ptr<PoliticalTakeoverWorldAccess> politics,std::unique_ptr<EconomicSupplyWorldAccess> supply,const DynamicStepInput* phase):politics_(std::move(politics)),supply_(std::move(supply)){
    if(!politics_)return;
    if(w.occupation_fiscal_enabled()||w.fiscal_taxpayer_continuity_enabled())taxpayer_.emplace(politics_->context());
    if(w.tasks.personal_advances)advances_.emplace(w.common,w.tasks,*w.tasks.personal_advances);
    economy_.emplace(EconomicTakeoverReadContext{w.common,w.local_economy,*w.catalog->local_economy,w.fiscal_settlement_enabled()?&w.fiscal_settlement:nullptr,w.fiscal_settlement_enabled()?w.catalog->fiscal_settlement.get():nullptr,w.public_relief_enabled()?&w.public_relief:nullptr,politics_->responsibilities(),&w.organization,w.catalog->organization.get(),phase,w.occupation_fiscal_enabled()?&w.occupation_fiscal_settlement:nullptr,taxpayer()});
    tasks_={w.catalog->tasks.get(),&politics_->responsibilities(),&*w.tasks.takeover_execution,advances_?&*advances_:nullptr,phase,issued_supply()};
}
Outcome<std::vector<EconomicSupplySuspension>> PoliticalTakeoverExecutionWorldAccess::supply_suspensions()const{
    if(!economy_)return {true,{}, {}};
    return economy_->phase?read_economic_supply_suspensions(*economy_,*economy_->phase):read_economic_supply_suspensions(*economy_);
}
Outcome<std::unique_ptr<PoliticalTakeoverExecutionWorldAccess>> PoliticalTakeoverExecutionWorldAccess::create(const WorldBundle& w,const DynamicStepInput* phase){
    auto evaluation=political_takeover_evaluation_tick(w.common,phase);if(!evaluation.ok)return {false,{},evaluation.error};
    std::unique_ptr<PoliticalTakeoverWorldAccess> politics;
    if(w.political_takeover_enabled()){
        if(!w.political_takeover_seed_complete||!w.economic_takeover_seed_complete||!w.tasks.takeover_execution||!w.catalog->task_takeover_execution_enabled)return {false,{},common_failure("MissingPoliticalTakeoverExecutionState").error};
        auto created=PoliticalTakeoverWorldAccess::create(w,*w.catalog->political_takeover,w.political_takeover,phase);if(!created.ok)return {false,{},created.error};politics=std::move(created.value);
    }else if(w.tasks.takeover_execution||w.catalog->task_takeover_execution_enabled)return {false,{},common_failure("UnexpectedPoliticalTakeoverProfile").error};
    std::unique_ptr<EconomicSupplyWorldAccess> supply;if(w.economic_supply_policies_enabled()){auto created=EconomicSupplyWorldAccess::create(w,phase);if(!created.ok)return {false,{},created.error};supply=std::move(created.value);}
    return {true,std::unique_ptr<PoliticalTakeoverExecutionWorldAccess>(new PoliticalTakeoverExecutionWorldAccess(w,std::move(politics),std::move(supply),phase)),{}};
}
}

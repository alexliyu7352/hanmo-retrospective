#include "resident/domains/military/military_model.hpp"
// 只替换原siege同一次apply，不另建占领账或从事后Owner重建转移前责任。
#include "politics_takeover_application.hpp"
#include "politics_takeover_capture.hpp"
#include "politics_takeover_detail.hpp"
#include "politics_takeover_territory.hpp"
#include "politics_takeover_production.hpp"
#include "resident/domains/military/occupation_applied_source.hpp"
namespace hanmo::resident {
Outcome<PoliticalTakeoverApplication> apply_political_military_takeover_in_transaction(CommonStore& common,MilitaryState& military,PoliticalTakeoverState& state,const PoliticalTakeoverApplicationContext& context,const StableId& occupation_id,const PoliticalTakeoverPendingApplication& apply_pending){
    const auto& original=context.original;
    const auto failure=[](const StableId& code)->Outcome<PoliticalTakeoverApplication>{return {false,{},common_failure(code).error};};
    if(&common!=&original.common||&military!=&original.military)return failure("PoliticalTakeoverCandidateMismatch");
    if((state.version!=1&&state.version!=2)||state.version!=context.catalog.version)return failure("UnsupportedPoliticalTakeoverVersion");
    if(state.version==2&&!context.actual_clock)return failure("PoliticalTakeoverActualClockRequired");
    const auto* occupation=takeover_detail::occupation(military,occupation_id);if(!occupation)return failure("UnknownMilitaryOccupation");
    const auto catalog_valid=validate_political_takeover_catalog(context.common_catalog,context.organization_catalog,context.politics_catalog,original.military_catalog,context.catalog);if(!catalog_valid.ok)return {false,{},catalog_valid.error};
    auto rule=political_takeover_rule_for_occupation(context.organization_catalog,context.catalog,*occupation);if(!rule.ok)return {false,{},rule.error};
    if(!rule.value){auto applied=apply_military_occupation(common,military,original.military_catalog,original.military_access,occupation_id);if(!applied.ok)return {false,{},applied.error};return {true,{std::move(applied.value),std::nullopt},{}};}
    // 幂等重入只返原军事结果，绝不二次登记pending或再次执行暂停回调。
    if(occupation->assets_applied.value_or(false)){
        const PoliticalTakeoverRegistration* registered=nullptr;for(const auto& row:state.registrations.rows())if(row.second.occupation_fact_id==occupation_id){if(registered)return failure("DuplicatePoliticalTakeoverRegistration");registered=&row.second;}
        if(!registered||registered->rule_id!=*rule.value)return failure("MissingPoliticalTakeoverRegistration");
        auto applied=apply_military_occupation(common,military,original.military_catalog,original.military_access,occupation_id);if(!applied.ok)return {false,{},applied.error};return {true,{std::move(applied.value),registered->id},{}};
    }
    auto captured=capture_political_takeover_responsibilities(common,original.economy,original.economy_catalog,original.fiscal,original.fiscal_catalog,original.relief,original.tasks,original.task_catalog,original.governance,original.envoy,military,occupation_id,original.occupation_fiscal,original.phase);if(!captured.ok)return {false,{},captured.error};
    // 有实际责任而无本域执行服务时在产权转移前拒绝，不能把pending当成已经适配。
    if(!captured.value.original().responsibilities.empty()&&!apply_pending)return failure("MissingPoliticalResponsibilityExecutionServices");
    std::optional<PreparedProductionResponsibilityChange> production_before;
    if(state.version==2){
        if(context.production&&(&context.production->economy!=&original.economy||&context.production->economy_catalog!=&original.economy_catalog))return failure("PoliticalProductionCandidateMismatch");
        auto now=read_organization_authority_now(common,context.actual_clock);if(!now.ok)return {false,{},now.error};
        auto prepared=prepare_political_production_change(common,captured.value.original().responsibilities,now.value,context.production);if(!prepared.ok)return {false,{},prepared.error};
        production_before=std::move(prepared.value);
    }
    auto applied=apply_military_occupation_with_source(common,military,original.military_catalog,original.military_access,common.state().identity,occupation_id,context.reservations);if(!applied.ok)return {false,{},applied.error};if(!applied.value.source)return failure("MissingPoliticalTakeoverAppliedSource");
    auto registration=register_political_takeover_in_transaction(common,state,context.catalog,military,captured.value,*applied.value.source,*rule.value,context.actual_clock);if(!registration.ok)return {false,{},registration.error};
    auto control=apply_political_takeover_control_in_candidate(common,context.politics,context.catalog,state,*applied.value.source,registration.value);if(!control.ok)return {false,{},control.error};
    auto pending=read_political_pending_responsibilities_in_candidate(common,state,context.catalog,*applied.value.source,registration.value);if(!pending.ok)return {false,{},pending.error};
    if(apply_pending){const auto* row=state.registrations.find(registration.value);const auto source=takeover_detail::registration_source(common,*row,*context.catalog.rules.find(row->rule_id));auto adapted=apply_pending(source,pending.value);if(!adapted.ok)return {false,{},adapted.error};}
    if(state.version==2){auto adopted=apply_political_production_change_in_candidate(common,state,std::move(production_before),{{"politics",political_takeover_pending_fact_kind,registration.value},2},context.production);if(!adopted.ok)return {false,{},adopted.error};}
    return {true,{std::move(applied.value.effect),registration.value},{}};
}
}

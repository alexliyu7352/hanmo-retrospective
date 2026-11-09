// 新政策不经旧财政1产生税债；实际金额另由同候选新财政段结算。
#include "occupation_fiscal_integration.hpp"
#include "world_bundle.hpp"
#include "resident/domains/local_economy/fiscal/occupation_fiscal_view.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_need_view.hpp"
#include "resident/domains/local_economy/views/local_economy_view.hpp"
namespace hanmo::resident {
OccupationFiscalWorldAccess::OccupationFiscalWorldAccess(const WorldBundle& w,std::unique_ptr<PoliticalTakeoverWorldAccess> takeover):takeover_(std::move(takeover)),context_{takeover_->context(),w.politics,*w.catalog->occupation_fiscal,w.occupation_fiscal,w.political_fiscal},policy_(context_){}
Outcome<std::unique_ptr<OccupationFiscalWorldAccess>> OccupationFiscalWorldAccess::create(const WorldBundle& w,const DynamicStepInput* phase){
    if(!w.occupation_fiscal_enabled()||!w.fiscal_settlement_enabled()||!w.catalog->occupation_fiscal_settlement_enabled||!w.occupation_fiscal_seed_complete||!w.occupation_fiscal_settlement_seed_complete)return {false,{},common_failure("OccupationFiscalProfileRequired").error};
    auto takeover=PoliticalTakeoverWorldAccess::create(w,*w.catalog->political_takeover,w.political_takeover,phase);if(!takeover.ok)return {false,{},takeover.error};
    return {true,std::unique_ptr<OccupationFiscalWorldAccess>(new OccupationFiscalWorldAccess(w,std::move(takeover.value))),{}};
}
OccupationFiscalSettlementWorldAccess::OccupationFiscalSettlementWorldAccess(const WorldBundle& w,std::unique_ptr<PoliticalTakeoverExecutionWorldAccess> takeover,std::unique_ptr<OccupationFiscalWorldAccess> policy,const DynamicStepInput* phase):world_(w),takeover_(std::move(takeover)),policy_(std::move(policy)),organizations_(w.services.organization_access()),legacy_(phase?std::make_unique<FiscalPolicyAccess>(w.common,*w.catalog->common,w.politics,w.political_fiscal,*w.catalog->political_fiscal,*phase,takeover_->fiscal()):std::make_unique<FiscalPolicyAccess>(w.common,*w.catalog->common,w.politics,w.political_fiscal,*w.catalog->political_fiscal,takeover_->fiscal())),context_{w.common,w.local_economy,*w.catalog->local_economy,organizations_,*legacy_,*w.catalog->fiscal_settlement,w.fiscal_settlement,policy_->policy(),takeover_->economy(),phase,takeover_->taxpayer()}{if(w.fiscal_taxpayer_continuity_enabled()){continuity_.emplace(FiscalTaxpayerContinuityContext{*takeover_->taxpayer(),w.fiscal_taxpayer_continuity,phase});context_.legacy_continuity=&*continuity_;}}
Outcome<std::unique_ptr<OccupationFiscalSettlementWorldAccess>> OccupationFiscalSettlementWorldAccess::create(const WorldBundle& w,const DynamicStepInput* phase){
    auto policy=OccupationFiscalWorldAccess::create(w,phase);if(!policy.ok)return {false,{},policy.error};
    auto takeover=PoliticalTakeoverExecutionWorldAccess::create(w,phase);if(!takeover.ok)return {false,{},takeover.error};
    return {true,std::unique_ptr<OccupationFiscalSettlementWorldAccess>(new OccupationFiscalSettlementWorldAccess(w,std::move(takeover.value),std::move(policy.value),phase)),{}};
}
Outcome<std::optional<OccupationFiscalCivilAssessment>> OccupationFiscalSettlementWorldAccess::occupation_fiscal_civil_period(const StableId& id)const{
    if(context_.phase)return {false,{},common_failure("OccupationFiscalCivilCommittedClockRequired").error};
    auto read=read_occupation_fiscal_period(context_,world_.occupation_fiscal_settlement,id);if(!read.ok)return {false,{},read.error};if(!read.value)return {true,std::nullopt,{}};
    const auto* period=world_.occupation_fiscal_settlement.periods.find(id);if(!period)return {false,{},common_failure("MissingOccupationFiscalPeriod",id).error};
    return {true,OccupationFiscalCivilAssessment{world_.common.state().identity,id,read.value->due_tick,period->policy},{}};
}
Status initialize_occupation_fiscal_world(WorldBundle& w){
    if(!w.occupation_fiscal_enabled())return common_success();auto access=OccupationFiscalSettlementWorldAccess::create(w);if(!access.ok)return {false,{},access.error};
    if(!w.occupation_fiscal_settlement.periods.empty()||!w.occupation_fiscal_settlement.assessments.empty()||!w.occupation_fiscal_settlement.payments.empty()||!w.occupation_fiscal_settlement.buckets.empty()||!w.occupation_fiscal_settlement.legacy_carry_claims.empty()||!w.occupation_fiscal_settlement.suspended_periods.empty())return common_failure("OccupationFiscalAlreadyInitialized");
    auto initial=make_initial_occupation_fiscal_settlement(access.value->context());if(!initial.ok)return {false,{},initial.error};w.occupation_fiscal_settlement=std::move(initial.value);return common_success();
}
Status validate_occupation_fiscal_world(const WorldBundle& w){
    const bool enabled=w.occupation_fiscal_enabled();
    auto layout=validate_occupation_fiscal_layout(enabled,w.catalog->identity.module_versions,w.occupation_fiscal_seed_complete?&w.occupation_fiscal:nullptr,w.occupation_fiscal_settlement_seed_complete?std::optional<Integer>{w.occupation_fiscal_settlement.version}:std::nullopt);if(!layout.ok)return layout;
    if(w.catalog->occupation_fiscal_settlement_enabled!=enabled||(w.catalog->occupation_fiscal&&w.catalog->occupation_fiscal->enabled&&!enabled))return common_failure("OccupationFiscalProfileMismatch");
    auto history=validate_occupation_fiscal_command_history(w.common,enabled);if(!history.ok)return history;
    if(!enabled){if(!w.occupation_fiscal.policies.empty()||!w.occupation_fiscal_settlement.periods.empty()||!w.occupation_fiscal_settlement.assessments.empty()||!w.occupation_fiscal_settlement.payments.empty()||!w.occupation_fiscal_settlement.buckets.empty()||!w.occupation_fiscal_settlement.legacy_carry_claims.empty()||!w.occupation_fiscal_settlement.suspended_periods.empty())return common_failure("UnexpectedOccupationFiscalState");return common_success();}
    auto access=OccupationFiscalWorldAccess::create(w);if(!access.ok)return {false,{},access.error};
    return validate_occupation_fiscal_receipts(access.value->context(),[](const CommitResult& result){return result.kind==political_occupation_fiscal_command_kind&&result.details?std::get_if<PoliticalFiscalEffects>(&result.details->data):nullptr;});
}
Status require_legacy_fiscal_world(const WorldBundle& w,const StableId& jurisdiction){
    if(!w.occupation_fiscal_enabled())return common_success();
    auto access=OccupationFiscalWorldAccess::create(w);if(!access.ok)return {false,{},access.error};
    return require_legacy_fiscal_before_occupation(access.value->context().takeover,jurisdiction);
}
Outcome<OccupationFiscalQuote> prepare_occupation_fiscal_world(const WorldBundle& w,const StableId& actor,const PoliticalSetOccupationTaxCommand& command){
    auto access=OccupationFiscalWorldAccess::create(w);if(!access.ok)return {false,{},access.error};auto quote=prepare_occupation_fiscal_command(access.value->context(),w.services,actor,command);if(!quote.ok)return quote;auto money=OccupationFiscalSettlementWorldAccess::create(w);if(!money.ok)return {false,{},money.error};const auto* district=w.catalog->politics->jurisdictions.find(command.jurisdiction_id);if(!district)return {false,{},common_failure("UnknownPoliticalJurisdiction").error};auto source=validate_occupation_fiscal_new_sources(money.value->context(),district->seat_place_id);if(!source.ok)return {false,{},source.error};return quote;
}
Outcome<PoliticalFiscalEffects> commit_occupation_fiscal_world(WorldBundle& w,const StableId& actor,const PoliticalSetOccupationTaxCommand& command,const OccupationFiscalQuote& quote){
    auto prepared=prepare_occupation_fiscal_world(w,actor,command);if(!prepared.ok)return {false,{},prepared.error};auto access=OccupationFiscalWorldAccess::create(w);if(!access.ok)return {false,{},access.error};return commit_occupation_fiscal_command_in_transaction(w.common,w.occupation_fiscal,access.value->context(),w.services,actor,command,quote);
}
Outcome<BoundaryValue> query_occupation_fiscal_settlement_world(const WorldBundle& w,const BoundaryValue& args){
    auto query=decode_local_economy_query(args,w.common.state().metadata.player_actor_id);if(!query.ok)return {false,{},query.error};auto access=OccupationFiscalSettlementWorldAccess::create(w);if(!access.ok)return {false,{},access.error};auto index=build_occupation_fiscal_settlement_index(access.value->context(),w.occupation_fiscal_settlement,w.organization);if(!index.ok)return {false,{},index.error};if(w.occupation_fiscal_needs_enabled())bind_fiscal_need_references(index.value,w.affairs);auto page=query_occupation_fiscal_settlement(access.value->context(),w.occupation_fiscal_settlement,index.value,query.value);if(!page.ok)return {false,{},page.error};return {true,encode_occupation_fiscal_settlement_page(page.value),{}};
}
Outcome<BoundaryValue> query_occupation_fiscal_world(const WorldBundle& w,const StableId& actor,const StableId& site){
    auto access=OccupationFiscalWorldAccess::create(w);if(!access.ok)return {false,{},access.error};return query_occupation_fiscal_entries(access.value->context(),w.services,actor,site);
}
}

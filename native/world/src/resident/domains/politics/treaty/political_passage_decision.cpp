#include "political_passage_decision.hpp"
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
RuleError failure(const char* code,ErrorKind kind=ErrorKind::rule){RuleError e;e.original={code,"politics/passage"};e.kind=kind;return e;}
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
Status source_refs(const std::vector<SourceRef>& refs){for(const auto& r:refs)if(!valid_source_ref(r))return {false,{},failure("PoliticalPassageSourceMissing")};return {true,{},{}};}
Outcome<Integer> tariff_amount(const PassageTariff& tariff,const TransportManifestRead& manifest){
    if(!valid_source_ref(tariff.source)||tariff.currency_id.empty()||tariff.payee_wallet_id.empty()||tariff.amount_per_unit<0||manifest.persons<0||manifest.weight<0)
        return {false,{},failure("InvalidPoliticalPassageTariff")};
    Integer units=0;
    switch(tariff.kind){
    case PassageTariffKind::fixed_per_crossing:units=1;break;
    case PassageTariffKind::per_person:units=manifest.persons;break;
    case PassageTariffKind::per_vessel:{
        std::set<StableId> seen;
        for(const auto& placement:manifest.vessel_placements){if(placement.vessel_id.empty()||!seen.insert(placement.vessel_id).second)return {false,{},failure("DuplicatePassageVesselPlacement")};}
        if(seen.size()>static_cast<std::uint64_t>(std::numeric_limits<Integer>::max()))return {false,{},failure("PoliticalPassageTariffOverflow")};
        units=static_cast<Integer>(seen.size());break;
    }
    case PassageTariffKind::per_weight_unit:
        if(tariff.weight_unit<=0)return {false,{},failure("InvalidPassageWeightUnit")};
        units=manifest.weight/tariff.weight_unit+(manifest.weight%tariff.weight_unit!=0);break;
    default:return {false,{},failure("UnsupportedPoliticalPassageTariff",ErrorKind::unsupported_capability)};
    }
    if(units&&tariff.amount_per_unit>std::numeric_limits<Integer>::max()/units)return {false,{},failure("PoliticalPassageTariffOverflow")};
    return {true,units*tariff.amount_per_unit,{}};
}
}
Outcome<PoliticalPassageDecision> evaluate_political_passage(const PoliticalPassageContext& context,const PassageRequest& request,const TransportManifestRead& manifest){
    if(!request.read_stamp||!manifest.read_stamp||!same_identity(context.identity,request.identity))return {false,{},failure("PoliticalPassageReadContextMismatch")};
    auto current=context.stamps.validate(*request.read_stamp,context.identity,context.read_mode,context.common,context.versions);
    if(!current.ok)return {false,{},current.error};
    current=context.stamps.validate(*manifest.read_stamp,context.identity,context.read_mode,context.common,context.versions);
    if(!current.ok)return {false,{},current.error};
    if(request.window.from_tick<0||request.window.until_tick<=request.window.from_tick||request.boundary.id.empty()||request.boundary.map_id.empty()||request.boundary.from_site_id.empty()||request.boundary.to_site_id.empty())return {false,{},failure("InvalidPoliticalPassageBoundary")};
    if(request.mode!="foot"&&request.mode!="army"&&request.mode!="boat")return {false,{},failure("UnsupportedPoliticalPassageMode",ErrorKind::unsupported_capability)};
    if(request.mode=="army"&&!request.army_id)return {false,{},failure("PoliticalPassageArmySourceRequired")};
    if(request.purpose==PassagePurpose::enter){
        if(!request.execution||(!std::holds_alternative<PersonalMovementExecution>(*request.execution)&&!std::holds_alternative<GroupMovementExecution>(*request.execution)))
            return {false,{},failure("PoliticalPassageExecutionRequired")};
    }
    if(request.manifest_basis.manifest_revision!=manifest.manifest_revision||request.manifest_basis.composition_revision!=manifest.composition_revision)
        return {false,{},failure("PoliticalPassageManifestChanged")};
    auto actual=context.sources.verify_actual_request(request,manifest);if(!actual.ok)return {false,{},actual.error};
    auto policy=context.sources.read_current_policy(request,manifest);if(!policy.ok)return {false,{},policy.error};
    if(!same_identity(policy.value.identity,context.identity)||!valid_source_ref(policy.value.admission_source))return {false,{},failure("PoliticalPassageSourceMissing")};
    for(const auto* refs:{&policy.value.authority_basis_refs,&policy.value.treaty_term_refs,&policy.value.local_delivery_source_refs}){auto valid=source_refs(*refs);if(!valid.ok)return {false,{},valid.error};}
    if(context.selected_source_schema!=1&&context.selected_source_schema!=2)return {false,{},failure("PoliticalPassageSourceSchemaUnsupported")};
    PoliticalPassageDecision result;
    result.source_schema_version=context.selected_source_schema;
    if(context.selected_source_schema==2){
        result.purpose_context=request.purpose_context;result.authority_basis=policy.value.authority_basis;result.requested_window=request.window;
    }
    result.identity=context.identity;result.boundary=request.boundary;result.subject=request.subject;result.mode=request.mode;
    result.entry_tick=request.window.from_tick;result.admission=policy.value.admission;result.requester=request.requester;
    result.admission_source=policy.value.admission_source;result.authority_basis_refs=policy.value.authority_basis_refs;
    result.treaty_term_refs=policy.value.treaty_term_refs;result.local_delivery_source_refs=policy.value.local_delivery_source_refs;
    result.visible_reason_code=policy.value.visible_reason_code;result.read_stamp=request.read_stamp;
    if(context.selected_source_schema==2){auto payload=validate_political_passage_authority_payload(result);if(!payload.ok)return {false,{},payload.error};}
    if(result.admission!=PassageAdmission::allowed)return {true,std::move(result),{}};
    if(policy.value.exemption_source){
        if(!valid_source_ref(*policy.value.exemption_source))return {false,{},failure("PoliticalPassageExemptionSourceMissing")};
        result.authority_basis_refs.push_back(*policy.value.exemption_source);
    }else if(const auto* tariff=policy.value.treaty_tariff?&*policy.value.treaty_tariff:policy.value.local_tariff?&*policy.value.local_tariff:nullptr){
        auto payee=context.sources.verify_tariff_payee(request.boundary,*tariff);if(!payee.ok)return {false,{},payee.error};
        auto amount=tariff_amount(*tariff,manifest);if(!amount.ok)return {false,{},amount.error};
        result.toll=PassageToll{tariff->currency_id,amount.value,tariff->payee_wallet_id,tariff->source};
    }
    return {true,std::move(result),{}};
}
}

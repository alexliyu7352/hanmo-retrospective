#include "resident/domains/military/military_model.hpp"
// 冷验证同时检查真实原责任全集、军事应用和逐决定公权，删除整行也不能规避责任。
#include "politics_takeover_detail.hpp"
#include "politics_takeover_catalog.hpp"
#include "politics_takeover_time2.hpp"
#include "resident/domains/politics/supply/politics_supply_adoptions.hpp"
namespace hanmo::resident {
Status validate_political_takeover_state(const PoliticalTakeoverContext& x){
    using namespace takeover_detail;
    const auto fail=[](const StableId& id){return common_failure("InvalidPoliticalTakeoverState",id);};
    if(bool(x.supply_adoptions)!=bool(x.supply_policies))return common_failure("MissingSupplyPolicyPair");
    if((x.catalog.version!=1&&x.catalog.version!=2)||x.state.version!=x.catalog.version)return common_failure("UnsupportedPoliticalTakeoverVersion");
    if(x.state.version==2){auto moments=validate_political_takeover_moments2(x);if(!moments.ok)return moments;}else{for(const auto& r:x.state.registrations.rows())if(r.second.registered_at)return common_failure("UnexpectedPoliticalTakeoverMoment",r.first);for(const auto& r:x.state.approvals.rows())if(r.second.approved_at)return common_failure("UnexpectedPoliticalTakeoverMoment",r.first);}
    if(!x.catalog.enabled&&(!x.state.registrations.empty()||!x.state.approvals.empty()||!x.state.decisions.empty()))return common_failure("UnexpectedPoliticalTakeoverState");
    auto evaluation=political_takeover_evaluation_tick(x.common,x.phase,x.catalog.version,x.actual_clock);if(!evaluation.ok)return {false,{},evaluation.error};
    std::set<StableId> covered,occupations;std::map<StableId,StableId> previous;
    const auto catalog_valid=validate_political_takeover_catalog(x.common_catalog,x.organization_catalog,x.politics_catalog,x.military_catalog,x.catalog);if(!catalog_valid.ok)return catalog_valid;
    for(const auto& row:x.state.registrations.rows()){
        const auto& r=row.second;const auto sequence=organization_instance_sequence(r.id,"political_takeover_pending"),original_sequence=organization_instance_sequence(r.occupation_fact_id,"military");
        if(row.first!=r.id||!sequence||!original_sequence||*sequence!=r.captured_next_instance_id||*original_sequence==INT64_MAX||r.captured_next_instance_id!=*original_sequence+1||*sequence>=x.common.state().next_instance_id||r.tick<0||r.tick>evaluation.value||r.source_identity.tick>r.tick||r.source_identity.tick<0||r.tick-r.source_identity.tick>1||r.source_identity.world_id!=x.common.state().identity.world_id||r.source_identity.revision>x.common.revision()||!header(x.common,r.id,political_takeover_pending_fact_kind)||!occupations.insert(r.occupation_fact_id).second)return fail(r.id);
        if(r.source_identity.execution_epoch<0||r.source_identity.revision<0)return fail(r.id);
        auto military=military_source(x,r);if(!military.ok)return military;
        const auto* rule=x.catalog.rules.find(r.rule_id);const auto* office=rule?x.organization_catalog.offices.find(rule->successor_office_id):nullptr;if(!office||office->organization_id!=r.successor_organization_id)return fail(r.id);
        const auto& fact=*occupation(x.military,r.occupation_fact_id);auto keys=x.originals.responsibility_keys(fact);if(!keys.ok)return {false,{},keys.error};
        std::set<decltype(key(PoliticalResponsibilityRef{}))> expected,actual;
        for(const auto& ref:keys.value)if(!valid_ref(ref)||!expected.insert(key(ref)).second)return common_failure("DuplicatePoliticalOriginalResponsibility",r.id);
        for(const auto& saved:r.responsibilities){if(!valid_ref(saved.responsibility)||!actual.insert(key(saved.responsibility)).second)return fail(r.id);auto source=x.originals.original_responsibility(fact,saved.responsibility);if(!source.ok)return {false,{},source.error};if(!compare_boundary(encode_political_original_responsibility(saved),encode_political_original_responsibility(source.value)).exact())return common_failure("PoliticalOriginalResponsibilityMismatch",saved.responsibility.responsibility_id);}
        if(expected!=actual)return common_failure("PoliticalResponsibilityCoverageMismatch",r.id);covered.insert(r.id);
    }
    // 本能力启用后的匹配接管必须原子登记pending，不能只提交军事资产变化。
    if(x.catalog.enabled)for(const auto& f:x.military.occupation_facts)if(f.assets_applied.value_or(false)){auto rule=political_takeover_rule_for_occupation(x.organization_catalog,x.catalog,f);if(!rule.ok)return {false,{},rule.error};if(rule.value&&!occupations.count(f.id))return common_failure("MissingPoliticalTakeoverRegistration",f.id);}
    for(const auto& row:x.state.approvals.rows()){
        const auto& a=row.second;const auto* r=x.state.registrations.find(a.registration_fact_id);const auto* rule=r?x.catalog.rules.find(r->rule_id):nullptr;const auto* grant=x.organization_catalog.grants.find(a.grant_id);const auto sequence=organization_instance_sequence(a.id,"political_takeover");
        if(row.first!=a.id||!r||!rule||(x.state.version==1&&!grant)||!sequence||*sequence>=x.common.state().next_instance_id||a.tick<r->tick||a.tick>(x.state.version==1?x.common.tick():evaluation.value)||a.authority.organization_id!=r->successor_organization_id||a.authority.office_id!=rule->successor_office_id||!header(x.common,a.id,political_takeover_fact_kind)||(x.state.version==1&&!grant_covers(*grant,*rule,*r,a.tick)))return fail(a.id);
        const auto* initial=x.organization_catalog.offices.find(a.authority.office_id);if(!initial||(x.state.version==1&&!has(initial->grant_ids,a.grant_id)))return fail(a.id);
        const auto last=previous.find(r->id);const auto prior=last==previous.end()?std::nullopt:std::optional<StableId>{last->second};if(a.previous_approval_fact_id!=prior)return common_failure("InvalidPoliticalApprovalChain",a.id);
        const auto before=organization_instance_sequence(prior?*prior:r->id,prior?"political_takeover":"political_takeover_pending");if(!before||*before>=*sequence)return fail(a.id);
        // 同tick内再次易手也按真实共同序号截断旧控制权，旧任职不能为已失辖区补签决定。
        const MilitaryOccupationFact* controlled=nullptr;Integer control_sequence=0;
        for(const auto& fact:x.military.occupation_facts)if(fact.garrison_id==rule->garrison_id&&fact.assets_applied.value_or(false)){
            auto number=organization_instance_sequence(fact.id,"military");if(!number)return fail(fact.id);if(*number>=*sequence)continue;
            if(fact.tick>a.tick)return common_failure("InvalidPoliticalApprovalControlTime",a.id);
            if(*number>control_sequence){control_sequence=*number;controlled=&fact;}
        }
        if(!controlled||controlled->id!=r->occupation_fact_id||controlled->to_organization_id!=a.authority.organization_id)return common_failure("PoliticalApprovalWithoutCurrentControl",a.id);
        auto authority=x.state.version==2?validate_political_takeover_approval_authority2(x,a):validate_historical_office_signer(x.common,x.organizations,x.organization_catalog,a.authority,a.tick,*sequence);if(!authority.ok)return authority;
        if(a.decision_fact_ids.size()!=r->responsibilities.size())return common_failure("PoliticalResponsibilityCoverageMismatch",a.id);
        Integer previous_sequence=*sequence;std::set<decltype(key(PoliticalResponsibilityRef{}))> keys;
        for(const auto& id:a.decision_fact_ids){const auto* record=x.state.decisions.find(id);const auto seq=organization_instance_sequence(id,"political_responsibility");if(!record||record->id!=id||record->approval_fact_id!=a.id||!seq||*seq<=previous_sequence||*seq>=x.common.state().next_instance_id||!header(x.common,id,political_takeover_decision_fact_kind)||!covered.insert(id).second)return fail(id);previous_sequence=*seq;
            const auto& d=record->decision;if(!keys.insert(key(d.responsibility)).second)return fail(id);const auto found=std::find_if(r->responsibilities.begin(),r->responsibilities.end(),[&](const auto& o){return key(o.responsibility)==key(d.responsibility);});if(found==r->responsibilities.end())return fail(id);
            PoliticalResponsibilityRequest request{d.responsibility,d.disposition,d.payment_wallet_id,d.payment_inventory_id,d.production_asset_ids};if(x.supply_adoptions){if(const auto* binding=x.supply_adoptions->bindings.find(id))request.supply_policy_fact_id=binding->policy_issue_fact_id;}auto derived=derive_political_supply_decision(x,*r,*found,request,a.id);if(!derived.ok)return {false,{},derived.error};derived.value.decision_fact_id=id;derived.value.takeover_fact_id=a.id;
            if(!compare_boundary(encode_political_responsibility_decision(d),encode_political_responsibility_decision(derived.value)).exact())return common_failure("InvalidPoliticalResponsibilityDecision",id);
        }
        previous[r->id]=a.id;covered.insert(a.id);
    }
    for(const auto& row:x.state.decisions.rows())if(!covered.count(row.first))return common_failure("OrphanPoliticalResponsibilityDecision",row.first);
    for(const auto& row:x.common.state().facts.rows())if(row.second.domain=="politics"&&(row.second.kind==political_takeover_pending_fact_kind||row.second.kind==political_takeover_fact_kind||row.second.kind==political_takeover_decision_fact_kind)&&!covered.count(row.first))return common_failure("MissingPoliticalTakeoverPayload",row.first);
    return validate_political_supply_adoption_bindings(x);
}
}

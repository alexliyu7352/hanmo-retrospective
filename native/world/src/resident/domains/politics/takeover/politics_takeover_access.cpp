#include "resident/domains/military/military_model.hpp"
// 内部执行读保留历史主体；当前控制变化只产生明确暂停结果，不改旧账。
#include "politics_takeover_detail.hpp"
namespace hanmo::resident {
// 选最后真正发生的覆盖登记，不按表排列或资产今天的Owner猜责任。
Outcome<std::optional<PoliticalResponsibilityExecution>> PoliticalTakeoverAccess::find_political_responsibility_for_execution(const PoliticalResponsibilityRef& ref,const WorldIdentity& identity) const{
    using namespace takeover_detail;const auto& x=context_;
    if(!same_identity(identity,x.common.state().identity))return {false,{},common_failure("PoliticalTakeoverRevisionMismatch").error};
    if(!valid_ref(ref))return {false,{},common_failure("InvalidPoliticalResponsibilityReference").error};
    auto valid=validate_political_takeover_state(x);if(!valid.ok)return {false,{},valid.error};
    const PoliticalTakeoverRegistration* selected=nullptr;Integer last_sequence=0;
    for(const auto& row:x.state.registrations.rows())if(std::any_of(row.second.responsibilities.begin(),row.second.responsibilities.end(),[&](const auto& o){return key(o.responsibility)==key(ref);})){
        const auto sequence=organization_instance_sequence(row.first,"political_takeover_pending");if(!sequence)return {false,{},common_failure("InvalidPoliticalTakeoverState",row.first).error};
        if(*sequence>last_sequence){last_sequence=*sequence;selected=&row.second;}
    }
    if(!selected)return {true,std::nullopt,{}};
    auto result=political_responsibility_for_execution(selected->id,ref,identity);if(!result.ok)return {false,{},result.error};return {true,std::move(result.value),{}};
}
Outcome<PoliticalTakeoverApproval> PoliticalTakeoverAccess::political_takeover_source(const StableId& id) const{
    const auto valid=validate_political_takeover_state(context_);if(!valid.ok)return {false,{},valid.error};
    const auto* a=context_.state.approvals.find(id);if(!a)return {false,{},common_failure("UnknownPoliticalTakeoverApproval",id).error};
    const auto& r=*context_.state.registrations.find(a->registration_fact_id);const auto& rule=*context_.catalog.rules.find(r.rule_id);
    PoliticalTakeoverApproval out;auto& s=out.source;s.source_identity=context_.common.state().identity;s.fact_id=a->id;s.occupation_fact_id=r.occupation_fact_id;s.jurisdiction_id=rule.jurisdiction_id;s.site_id=rule.site_id;s.former_organization_id=r.former_organization_id;s.successor_organization_id=r.successor_organization_id;s.authority=a->authority;s.grant_id=a->grant_id;s.occupation_tick=r.tick;s.effective_tick=a->tick;s.public_wallet_ids=r.public_wallet_ids;s.public_inventory_ids=r.public_inventory_ids;s.occupation_rule=rule.occupation_rule;
    for(const auto& decision:a->decision_fact_ids)out.decisions.push_back(context_.state.decisions.find(decision)->decision);
    return {true,std::move(out),{}};
}
Outcome<PoliticalResponsibilityExecution> PoliticalTakeoverAccess::political_responsibility_for_execution(const StableId& registration,const PoliticalResponsibilityRef& ref,const WorldIdentity& identity) const{
    using namespace takeover_detail;const auto& x=context_;
    if(!same_identity(identity,x.common.state().identity))return {false,{},common_failure("PoliticalTakeoverRevisionMismatch").error};
    const auto valid=validate_political_takeover_state(x);if(!valid.ok)return {false,{},valid.error};
    const auto* r=x.state.registrations.find(registration);if(!r)return {false,{},common_failure("UnknownPoliticalTakeover",registration).error};
    if(std::none_of(r->responsibilities.begin(),r->responsibilities.end(),[&](const auto& o){return key(o.responsibility)==key(ref);}))return {false,{},common_failure("UnknownPoliticalResponsibility",ref.responsibility_id).error};
    PoliticalResponsibilityExecution out;out.registration_fact_id=registration;out.responsibility=ref;out.registration=registration_source(x.common,*r,*x.catalog.rules.find(r->rule_id));out.code="PoliticalResponsibilityPending";for(const auto& source:r->responsibilities)if(key(source.responsibility)==key(ref))out.original_source_fact_ids=source.source_fact_ids;
    const auto* approval=latest(x.state,registration);if(!approval)return {true,std::move(out),{}};
    for(const auto& id:approval->decision_fact_ids){const auto& d=x.state.decisions.find(id)->decision;if(key(d.responsibility)==key(ref)){out.decision=d;break;}}
    if(!out.decision)return {false,{},common_failure("PoliticalResponsibilityCoverageMismatch").error};
    out.status=PoliticalResponsibilityExecutionStatus::suspended;out.code="PoliticalResponsibilitySuspended";
    if(out.decision->disposition==PoliticalResponsibilityDisposition::suspend||out.decision->disposition==PoliticalResponsibilityDisposition::settlement_required)return {true,std::move(out),{}};
    const auto& rule=*x.catalog.rules.find(r->rule_id);auto control=military_garrison_owner(x.common,x.military,x.military_catalog,x.military_access,rule.garrison_id);if(!control.ok)return {false,{},control.error};
    if(control.value.occupation_id!=r->occupation_fact_id||control.value.organization_id!=r->successor_organization_id){out.code="PoliticalTakeoverControlChanged";return {true,std::move(out),{}};}
    const auto* org=x.organizations.organizations.find(r->successor_organization_id);if(!org)return {false,{},common_failure("UnknownOrganization",r->successor_organization_id).error};if(org->status!=OrganizationStatus::active){out.code="PoliticalSuccessorUnavailable";return {true,std::move(out),{}};}
    const bool assumed=out.decision->disposition==PoliticalResponsibilityDisposition::assume_by_successor;
    if(!assumed){
        // 多次易手后“原执行”仍认该责任首次捕获的逐容器Owner，不认最近的中间接管方。
        for(const auto& original:r->responsibilities)if(key(original.responsibility)==key(ref))for(const auto& container:original.containers)if(container.selected){
            const Owner* expected=nullptr;Integer first_sequence=INT64_MAX;
            for(const auto& registration_row:x.state.registrations.rows())for(const auto& source:registration_row.second.responsibilities)if(key(source.responsibility)==key(ref))for(const auto& saved:source.containers)if(saved.selected&&saved.id==container.id&&saved.inventory==container.inventory&&saved.original_owner){const auto sequence=organization_instance_sequence(registration_row.first,"political_takeover_pending");if(sequence&&*sequence<first_sequence){first_sequence=*sequence;expected=&*saved.original_owner;}}
            const auto* wallet=container.inventory?nullptr:x.common.wallet(container.id);const auto* inventory=container.inventory?x.common.inventory(container.id):nullptr;const Owner* actual=wallet?&wallet->owner:inventory?&inventory->owner:nullptr;
            if(!expected||!actual)return {false,{},common_failure("InvalidPoliticalOriginalAccountSource",container.id).error};
            if(!owner_equal(*actual,*expected)){out.code="PoliticalExecutionAccountChanged";return {true,std::move(out),{}};}
        }
        out.status=PoliticalResponsibilityExecutionStatus::approved;out.code.clear();return {true,std::move(out),{}};
    }
    const auto& owner=r->successor_organization_id;
    for(const auto& id:r->public_wallet_ids){const auto* account=x.common.wallet(id);if(!account)return {false,{},common_failure("MissingPoliticalPublicWallet",id).error};if(!owner_is(account->owner,owner)){out.code="PoliticalExecutionAccountChanged";return {true,std::move(out),{}};}}
    for(const auto& id:r->public_inventory_ids){const auto* account=x.common.inventory(id);if(!account)return {false,{},common_failure("MissingPoliticalPublicInventory",id).error};if(!owner_is(account->owner,owner)){out.code="PoliticalExecutionAccountChanged";return {true,std::move(out),{}};}}
    out.status=PoliticalResponsibilityExecutionStatus::approved;out.code.clear();return {true,std::move(out),{}};
}
}

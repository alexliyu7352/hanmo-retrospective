#include "resident/domains/military/military_model.hpp"
// 本域共同守门与规范引用；不从当前资产Owner倒推原责任主体。
#pragma once
#include "politics_takeover_rules.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <algorithm>
#include <set>
#include <tuple>
namespace hanmo::resident::takeover_detail {
inline bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
inline auto key(const PoliticalResponsibilityRef& r){return std::make_tuple(static_cast<int>(r.kind),r.responsibility_id,r.unit_id,r.asset_id);}
inline bool valid_ref(const PoliticalResponsibilityRef& r){return static_cast<int>(r.kind)>=static_cast<int>(PoliticalResponsibilityKind::production_unit)&&static_cast<int>(r.kind)<=static_cast<int>(PoliticalResponsibilityKind::task_contract)&&!r.responsibility_id.empty()&&(r.kind==PoliticalResponsibilityKind::production_asset_use?(r.unit_id&&!r.unit_id->empty()&&r.asset_id==std::optional<StableId>{r.responsibility_id}):(!r.unit_id&&!r.asset_id));}
inline bool has(const std::vector<StableId>& ids,const StableId& id){return std::find(ids.begin(),ids.end(),id)!=ids.end();}
inline bool owner_is(const Owner& owner,const StableId& organization){return owner.kind==Owner::Kind::organization&&owner.id==organization;}
inline bool header(const CommonStore& c,const StableId& id,const StableId& kind){const auto* f=c.state().facts.find(id);return f&&f->id==id&&f->domain=="politics"&&f->kind==kind;}
// 原目录grant必须独自覆盖本次全部公库和场所。
inline bool grant_covers(const Grant& g,const PoliticalTakeoverRule& rule,const PoliticalTakeoverRegistration& r,Integer tick){
    if(g.office_id!=rule.successor_office_id||g.amount_limit<0||tick<g.valid_from||(g.valid_until&&tick>*g.valid_until)||!has(g.allowed_command_kinds,political_takeover_command_kind))return false;
    if(std::none_of(g.scopes.begin(),g.scopes.end(),[&](const auto& s){return s.kind==ScopeRef::Kind::site&&s.id==rule.site_id;}))return false;
    for(const auto& id:r.public_wallet_ids)if(!has(g.wallet_ids,id))return false;
    for(const auto& id:r.public_inventory_ids)if(!has(g.inventory_ids,id))return false;
    return true;
}
inline const PoliticalTakeoverApprovalRecord* latest(const PoliticalTakeoverState& s,const StableId& registration){const PoliticalTakeoverApprovalRecord* out=nullptr;Integer last=0;for(const auto& row:s.approvals.rows())if(row.second.registration_fact_id==registration){auto number=organization_instance_sequence(row.first,"political_takeover");if(number&&*number>last){last=*number;out=&row.second;}}return out;}
inline const MilitaryOccupationFact* occupation(const MilitaryState& s,const StableId& id){const auto f=std::find_if(s.occupation_facts.begin(),s.occupation_facts.end(),[&](const auto& v){return v.id==id;});return f==s.occupation_facts.end()?nullptr:&*f;}
// 本人在原接管场所履行接任Office时才有该责任账的制度性知情；任意ID不开放远方查账。
inline bool known_to_actor(const PoliticalTakeoverContext& x,const StableId& actor,const PoliticalTakeoverRegistration& registration){
    const auto* rule=x.catalog.rules.find(registration.rule_id);const auto* person=x.common.actor(actor);const auto* office=rule?x.organizations.offices.find(rule->successor_office_id):nullptr;const auto* organization=x.organizations.organizations.find(registration.successor_organization_id);
    if(!x.catalog.enabled||!person||!person->alive||!rule||!office||!organization||organization->status!=OrganizationStatus::active||office->organization_id!=registration.successor_organization_id||office->holder_actor_id!=actor)return false;
    auto resolved=x.common.resolved_location(actor);const auto* location=resolved.ok?std::get_if<SettlementLocation>(&resolved.value):nullptr;return location&&location->place_id==rule->site_id;
}
// 同一已验证登记产生有限来源视图，不复制军事钱粮账。
inline PoliticalTakeoverRegistrationSource registration_source(const CommonStore& c,const PoliticalTakeoverRegistration& r,const PoliticalTakeoverRule& rule){PoliticalTakeoverRegistrationSource out;out.source_identity=c.state().identity;out.registration_fact_id=r.id;out.occupation_fact_id=r.occupation_fact_id;out.jurisdiction_id=rule.jurisdiction_id;out.site_id=rule.site_id;out.former_organization_id=r.former_organization_id;out.successor_organization_id=r.successor_organization_id;out.occupation_tick=r.tick;out.public_wallet_ids=r.public_wallet_ids;out.public_inventory_ids=r.public_inventory_ids;out.occupation_rule=rule.occupation_rule;return out;}
// 稳定时钟下核实际军事链；历史有效不要求它仍是今日控制者。
inline Status military_source(const PoliticalTakeoverContext& x,const PoliticalTakeoverRegistration& r){
    const auto* f=occupation(x.military,r.occupation_fact_id);const auto* rule=x.catalog.rules.find(r.rule_id);
    if(!f||!rule||!f->assets_applied.value_or(false)||!f->asset_manifest||f->id!=r.occupation_fact_id||f->tick!=r.tick||f->from_organization_id!=r.former_organization_id||f->to_organization_id!=r.successor_organization_id||f->garrison_id!=rule->garrison_id||f->stronghold_id!=rule->stronghold_id||f->public_wallet_ids!=r.public_wallet_ids||f->public_inventory_ids!=r.public_inventory_ids)return common_failure("InvalidPoliticalTakeoverMilitarySource",r.id);
    auto evaluation=political_takeover_evaluation_tick(x.common,x.phase,x.catalog.version,x.actual_clock);if(!evaluation.ok)return {false,{},evaluation.error};
    return validate_occupation_receipt(x.common,x.military,x.military_catalog,x.military_access,f->id,evaluation.value);
}
// 根据原责任生成完整决定；玩家不能提交替换受益人或私有收款容器。
inline Outcome<PoliticalResponsibilityDecision> derive(const PoliticalOriginalResponsibility& original,const PoliticalResponsibilityRequest& request,const StableId& successor,const StableId& former){
    if(key(original.responsibility)!=key(request.responsibility))return {false,{},common_failure("PoliticalResponsibilityMismatch").error};
    if(request.disposition!=PoliticalResponsibilityDisposition::continue_under_original&&request.disposition!=PoliticalResponsibilityDisposition::suspend&&request.disposition!=PoliticalResponsibilityDisposition::assume_by_successor&&request.disposition!=PoliticalResponsibilityDisposition::settlement_required)return {false,{},common_failure("UnknownPoliticalResponsibilityDisposition").error};
    PoliticalResponsibilityDecision d;d.responsibility=original.responsibility;d.disposition=request.disposition;d.original_principal=original.principal;d.original_assignee=original.assignee;d.original_beneficiaries=original.beneficiaries;d.original_delivery_beneficiaries=original.delivery_beneficiaries;d.original_assets=original.assets;d.collection_beneficiaries=original.beneficiaries;
    const bool assume=request.disposition==PoliticalResponsibilityDisposition::assume_by_successor;
    if(!assume&&(request.payment_wallet_id||request.payment_inventory_id||!request.production_asset_ids.empty()))return {false,{},common_failure("UnexpectedPoliticalExecutionAccounts").error};
    if(request.disposition==PoliticalResponsibilityDisposition::continue_under_original)d.execution_principal=original.principal;
    if(!assume)return {true,std::move(d),{}};
    if(original.responsibility.kind==PoliticalResponsibilityKind::supply_source)return {false,{},common_failure("PoliticalSupplySuccessorPolicyRequired").error};
    if(original.responsibility.kind==PoliticalResponsibilityKind::fiscal_assessment)return {false,{},common_failure("PoliticalFiscalReceivableAssignmentRequired").error};
    if(!original.protected_custody.empty())return {false,{},common_failure("PoliticalTaskExecutionAdaptationRequired").error};
    // 任务只承接原公库资金/报酬的同ID执行，不转交受款、托管或货物受益权。
    if(original.responsibility.kind==PoliticalResponsibilityKind::task_contract){
        // 静态任务与动态合同的报酬路径不同，未接静态支付适配时不预先授可执行权。
        if(std::none_of(original.record_sources.begin(),original.record_sources.end(),[&](const auto& source){return (source.kind=="contract"||source.kind=="static_task_cash_v2")&&source.id==original.responsibility.responsibility_id;}))return {false,{},common_failure("PoliticalTaskExecutionAdaptationRequired").error};
        if(request.payment_wallet_id||request.payment_inventory_id||!request.production_asset_ids.empty())return {false,{},common_failure("UnexpectedPoliticalExecutionAccounts").error};
        if(!original.principal||original.principal->kind!=SubjectKind::organization||original.principal->id.empty()||!original.assignee||original.assignee->id.empty()||!original.assets.empty()||!original.beneficiaries.empty())return {false,{},common_failure("InvalidPoliticalTaskResponsibility").error};
        std::set<std::pair<StableId,StableId>> roles;
        for(const auto& c:original.containers)if(c.selected){
            if(c.inventory||(c.role!="funding"&&c.role!="reward"))return {false,{},common_failure("PoliticalTaskExecutionAdaptationRequired",c.id).error};
            if(c.id.empty()||!c.original_owner||former.empty()||!owner_is(*c.original_owner,former)||!roles.emplace(c.role,c.id).second)return {false,{},common_failure("InvalidPoliticalTaskExecutionAccount",c.id).error};
            d.execution_accounts.push_back({c.role,c.id,c.id,false});
        }
        if(d.execution_accounts.empty())return {false,{},common_failure("PoliticalTaskPaymentAccountRequired").error};
        d.execution_principal=SubjectRef{SubjectKind::organization,successor};return {true,std::move(d),{}};
    }
    for(const auto& c:original.containers)if(c.selected&&c.role=="receipt")return {false,{},common_failure("PoliticalBeneficiaryAssignmentRequired",c.id).error};
    d.execution_principal=SubjectRef{SubjectKind::organization,successor};
    const auto account=[&](const std::optional<StableId>& requested,bool inventory)->bool{
        if(!requested)return true;
        return std::any_of(original.containers.begin(),original.containers.end(),[&](const auto& c){return c.selected&&c.inventory==inventory&&c.role=="payment"&&c.id==*requested;});
    };
    if(!account(request.payment_wallet_id,false)||!account(request.payment_inventory_id,true))return {false,{},common_failure("PoliticalPaymentAccountOutsideResponsibility").error};
    d.payment_wallet_id=request.payment_wallet_id;d.payment_inventory_id=request.payment_inventory_id;
    for(const auto& c:original.containers)if(c.selected&&c.role=="payment"){
        auto& target=c.inventory?d.payment_inventory_id:d.payment_wallet_id;
        if(target&&*target!=c.id)return {false,{},common_failure("PoliticalAmbiguousPaymentAccount").error};target=c.id;
    }
    std::vector<StableId> assets;for(const auto& a:original.assets)if(a.kind==PoliticalResponsibilityAssetSource::Kind::production_asset)assets.push_back(a.id);
    std::sort(assets.begin(),assets.end());auto requested=request.production_asset_ids;std::sort(requested.begin(),requested.end());
    if(std::adjacent_find(requested.begin(),requested.end())!=requested.end()||requested!=assets)return {false,{},common_failure("PoliticalProductionUseCoverageMismatch").error};
    d.production_asset_ids=std::move(assets);return {true,std::move(d),{}};
}
}

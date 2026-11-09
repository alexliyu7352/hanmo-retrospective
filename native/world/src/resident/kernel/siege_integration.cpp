// 同候选内先核逐将事实，再交接军资人口和占领；失败由外层事务整批丢弃。
#include "siege_integration.hpp"
#include "resident/domains/military/siege_world.hpp"
#include "resident/domains/military/battle_world.hpp"
#include "resident/domains/organization/organization_internal.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
// 未清私人任务和明确职事承诺不能被军事交接隐式抹除。
Status actor_commitments(const WorldBundle& w,const std::vector<StableId>& actors){
    for(const auto& actor:actors){
        for(const auto& row:w.tasks.tasks.rows())if(row.second.assignee==actor&&(row.second.status=="active"||row.second.status=="completed"))return common_failure("SiegeIntegrationTaskCommitment",row.first);
        for(const auto& row:w.tasks.contracts.rows())if(row.second.status!=ContractStatus::settled&&((row.second.assignee.kind==SubjectKind::actor&&row.second.assignee.id==actor)||row.second.executor_actor_id==actor))return common_failure("SiegeIntegrationContractCommitment",row.first);
        if(organization_detail::personally_committed(w.common,w.organization,&w.services,actor))return common_failure("SiegeIntegrationPersonalCommitment",actor);
        for(const auto& row:w.organization.duties.rows())if(row.second.actor_id==actor&&row.second.active&&!row.second.commitment_ids.empty())return common_failure("SiegeIntegrationDutyCommitment",row.first);
    }
    return common_success();
}
// 提议中只遍历实际转入人员，既有行政引用仍保留。
Status commitments(const WorldBundle& w,const SiegeIntegrationProposal& proposal){for(const auto& army:proposal.defender_armies){auto checked=actor_commitments(w,army.member_actor_ids);if(!checked.ok)return checked;}return common_success();}
// ReadContext只在access仍在栈上时使用，避免保存悬空的服务引用。
SiegeIntegrationReadContext read(const WorldBundle& w,const MilitaryAccess& access){return {w.common,w.military,*w.catalog->military,access,w.military.siege_integration,w.catalog->military->siege_integration};}
}
// 预览使用只读人口适配；缺少能力时仅允许没有登记人口的条款。
MilitaryIntegrationAccess siege_population_read_access(const WorldBundle& w){MilitaryIntegrationAccess out;out.preview_actor_commitments=[&w](const std::vector<StableId>& actors){return actor_commitments(w,actors);};out.preview_population=[&w](const SiegeIntegrationProposal& p){auto checked=commitments(w,p);if(!checked.ok)return checked;if(w.population_military_enabled())return preview_population_military_integration(w.population_read(),p);return p.terms.population_policy_ids.empty()?common_success():common_failure("SiegeIntegrationPopulationAdapterRequired");};return out;}
// 应用回调复用人口owner，不直接修改家户、工资或cohort。
MilitaryIntegrationAccess siege_population_access(WorldBundle& w){auto out=siege_population_read_access(w);if(w.population_military_enabled())out.apply_population=[&w](const MilitaryIntegrationFact& f){auto context=w.population_write();return apply_population_military_integration(context,f);};return out;}
// 最终报价再次核当前余额、批次和容量，提议语义不因此重签。
Outcome<SiegeIntegrationExecutionQuote> preview_siege_handover(const WorldBundle& w,const StableId& actor,const SiegeIntegrationApply& command){
    if(!w.siege_integration_enabled())return {false,{},common_failure("SiegeIntegrationCapabilityRequired",{},ErrorKind::unsupported_capability).error};
    auto ia=w.services.interaction_access();auto ma=w.services.military_access();ma.interaction_access=&ia;
    const auto* proposal=siege_integration_proposal(w.military.siege_integration,command.proposal_id);
    if(!proposal||proposal->proposal_key!=command.proposal_key)return {false,{},common_failure("SiegeIntegrationProposalMismatch").error};
    if(proposal->proposer_actor_id!=actor)return {false,{},common_failure("SiegeIntegrationProposerRequired").error};
    auto checked=validate_siege_integration_acceptances(read(w,ma),*proposal);if(!checked.ok)return {false,{},checked.error};
    auto access=siege_population_read_access(w);auto quote=preview_military_integration(w.common,w.military,*w.catalog->military,ma,w.military.siege_integration,*proposal,access);if(!quote.ok)return {false,{},quote.error};
    return {true,{proposal->id,proposal->proposal_key,std::move(quote.value)},{}};
}
// 已准备资源版本仍由资产primitive核验；任何后项拒绝都不能留下半次占领。
Outcome<SiegeIntegrationEffect> apply_siege_handover(WorldBundle& w,const StableId& actor,const SiegeIntegrationApply& command,const SiegeIntegrationExecutionQuote& quote){
    auto current=preview_siege_handover(w,actor,command);if(!current.ok)return {false,{},current.error};
    if(quote.proposal_id!=command.proposal_id||quote.proposal_key!=command.proposal_key)return {false,{},common_failure("SiegeIntegrationProposalMismatch").error};
    auto ia=w.services.interaction_access();auto ma=w.services.military_access();ma.interaction_access=&ia;auto access=siege_population_access(w);
    const auto proposal=*siege_integration_proposal(w.military.siege_integration,command.proposal_id);
    auto moved=apply_military_integration(w.common,w.military,*w.catalog->military,ma,w.military.siege_integration,proposal,quote.assets,access);if(!moved.ok)return {false,{},moved.error};
    w.common.bind_membership(&w.services,bool(w.catalog->field_battle));for(const auto& army:proposal.defender_armies)for(const auto& person:army.member_actor_ids)w.common.invalidate({ResourceKey::Kind::actor,person});
    auto finished=finish_siege_integration(w.common,w.military,*w.catalog->military,ma,moved.value,w.common.tick());if(!finished.ok)return {false,{},finished.error};
    auto changed=apply_military_actor_changes(w.common,w.military,*w.catalog->military,ma,finished.value.actor_changes);if(!changed.ok)return {false,{},changed.error};
    for(const auto& journey:finished.value.journey_requests){auto begun=begin_requested_military_journey(w,journey);if(!begun.ok)return {false,{},begun.error};}
    for(auto& attempt:w.military.siege_integration.attempts)if(attempt.proposal_id==proposal.id){attempt.status="completed";attempt.reason.clear();attempt.completion_source_id=moved.value;}
    SiegeIntegrationEffect effect;effect.proposal_id=proposal.id;effect.proposal_key=proposal.proposal_key;effect.status="completed";effect.completion_source_id=moved.value;return {true,std::move(effect),{}};
}
// 半日末每围城最多一名真实NPC裁定，提供可保存的部分决定边界。
Status run_siege_integration_decisions(WorldBundle& w){
    if(!w.siege_integration_enabled())return common_success();auto ia=w.services.interaction_access();auto ma=w.services.military_access();ma.interaction_access=&ia;
    SiegeIntegrationWriteContext context{w.common,w.military,*w.catalog->military,ma,w.military.siege_integration,w.catalog->military->siege_integration};
    auto refreshed=refresh_siege_integration_attempts(context.read(),w.military.siege_integration);if(!refreshed.ok)return refreshed;
    std::vector<StableId> pending;for(const auto& attempt:w.military.siege_integration.attempts)if(attempt.status=="pending")pending.push_back(attempt.proposal_id);
    for(const auto& id:pending){const auto* proposal=siege_integration_proposal(w.military.siege_integration,id);if(!proposal)return common_failure("UnknownSiegeIntegrationProposal",id);auto ready=commitments(w,*proposal);if(!ready.ok)continue;
        for(const auto& officer:proposal->required_officers){if(officer.actor_id==w.common.state().metadata.player_actor_id)continue;auto policy=preview_siege_integration_npc(context.read(),id,officer.actor_id);if(!policy.ok)return {false,{},policy.error};if(policy.value.decision=="wait")continue;auto applied=apply_siege_integration_npc(context,id,officer.actor_id);if(!applied.ok)return {false,{},applied.error};break;}
    }
    return common_success();
}
// 现有军政计划在真实任命后先请求原征募行动；人数和劳力来自明确许可及当前家户预备。
std::optional<CommandPayload> siege_population_enrollment(const WorldBundle& w,const StableId& actor,const ArmyForm& form){
    if(!w.siege_integration_enabled()||!w.population_military_enabled())return {};
    for(const auto& id:form.unit_ids){const MilitaryUnitDefinition* definition=nullptr;for(const auto& garrison:w.catalog->military->garrisons)for(const auto& unit:garrison.units)if(unit.id==id)definition=&unit;if(!definition||!definition->population_source_id)continue;const MilitaryUnitRecord* unit=nullptr;for(const auto& row:w.military.military_units)if(row.id==id)unit=&row;if(!unit||!unit->member_ids.empty())continue;
        auto* source=w.catalog->population_military->sources.find(*definition->population_source_id);auto* reserve=source?w.local_economy.cohorts.find(source->reserve_cohort_id):nullptr;auto* consent=source?w.catalog->population_military->consents.find(source->service_consent_source_id):nullptr;auto* office=source?w.organization.offices.find(source->organization_office_id):nullptr;
        if(!source||!reserve||!consent||!office||office->holder_actor_id!=actor)continue;const auto count=std::min({source->capacity,reserve->headcount,consent->max_active_count});if(count<=0)continue;
        PopulationEnrollCommand enroll;enroll.authority={source->organization_id,source->organization_office_id,office->epoch,actor};enroll.source_id=source->id;enroll.count=count;enroll.labor_count=std::min(count,reserve->labor_capacity);
        return CommandPayload{"population_enroll",PopulationMilitaryCommand{std::move(enroll)}};
    }return {};
}
// 世界冷准入统一核版本能力、永久来源和当前域一致性。
Status validate_siege_integration_world(const WorldBundle& w){
    auto ia=w.services.interaction_access();auto ma=w.services.military_access();ma.interaction_access=&ia;auto checked=validate_siege_integration_state(read(w,ma));if(!checked.ok)return checked;
    for(const auto& row:w.common.state().results.rows())if(row.second&&row.second->details)if(const auto* effect=std::get_if<SiegeIntegrationEffect>(&row.second->details->data)){
        if(!w.siege_integration_enabled())return common_failure("SiegeIntegrationCapabilityRequired",row.first);
        const auto* proposal=siege_integration_proposal(w.military.siege_integration,effect->proposal_id);if(!proposal||proposal->proposal_key!=effect->proposal_key)return common_failure("SiegeIntegrationReceiptSourceMismatch",row.first);
        if(row.second->kind=="siege_integration_apply"){auto found=std::find_if(w.military.integration_facts.begin(),w.military.integration_facts.end(),[&](const auto& f){return f.id==effect->completion_source_id&&f.proposal_id==proposal->id;});if(found==w.military.integration_facts.end())return common_failure("SiegeIntegrationReceiptSourceMismatch",row.first);}
        else{auto action=std::find_if(w.military.siege_integration.actor_actions.begin(),w.military.siege_integration.actor_actions.end(),[&](const auto& f){return f.id==effect->actor_action_source_id;});const auto command=row.second->kind=="siege_strategy"?StableId("siege_integration_propose"):row.second->kind;if(action==w.military.siege_integration.actor_actions.end()||action->proposal_id!=proposal->id||action->proposal_key!=effect->proposal_key||action->command!=command||action->decision_source_id!=effect->decision_source_id)return common_failure("SiegeIntegrationReceiptSourceMismatch",row.first);}
    }return common_success();
}
}

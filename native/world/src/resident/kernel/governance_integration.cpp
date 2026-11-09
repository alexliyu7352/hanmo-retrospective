// 具名安民入口不创建替代任务；每个preview只读，apply复验后借原三域内层事务。
#include "governance_integration.hpp"
#include "resident/domains/politics/governance_replies/political_governance_invitation_replies.hpp"
namespace hanmo::resident {
namespace {
// 完整世界身份阻止旧控件跨新局或读档执行；同候选仍再次验证真实资格与资源。
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
// 原权源先于包装器构造并覆盖一次实际调用；不把临时原读引用交给返回对象。
class AuthorityScope final {
public:
    explicit AuthorityScope(const GovernanceReadContext& c)
      :original_(c.common,c.politics,c.governance,c.governance_catalog,c.organization,c.organization_catalog,c.activity,c.agencies){
        if(c.invitation_replies)closed_.emplace(c.common,original_,*c.invitation_replies);
    }
    AuthorityScope(const AuthorityScope&)=delete;
    AuthorityScope& operator=(const AuthorityScope&)=delete;
    const GovernanceAuthorityRead& get()const{return closed_?static_cast<const GovernanceAuthorityRead&>(*closed_):original_;}
private:
    PoliticsGovernanceAccess original_;std::optional<GovernanceInvitationAcceptanceAccess> closed_;
};
// 成本匹配使用S5唯一家庭计划；政治Office、Grant和实际当面人物仍由S6决定。
Status issue_preview(const GovernanceReadContext& c,const StableId& actor,const PoliticalGovernanceCommissionCommand& command){auto valid=preview_governance_commission(c.common,c.common_catalog,c.politics,c.governance_catalog,c.organization,c.organization_catalog,c.activity,actor,command);if(!valid.ok)return valid;return validate_public_relief_commission_plan(c.common,c.economy,c.relief_catalog,c.politics,c.governance_catalog,command);}
}
GovernanceReadContext GovernanceWriteContext::read()const{return {common,common_catalog,politics,politics_catalog,governance,governance_catalog,organization,organization_catalog,economy,relief_catalog,relief,tasks,governance_tasks,activity,nullptr,nullptr,invitation_replies,agencies};}
bool governance_command_kind(const StableId& kind){return kind=="political_commission_governance"||kind=="governance_accept"||kind=="governance_cancel";}
StableId governance_command_kind(const GovernanceCommand& command){if(std::holds_alternative<PoliticalGovernanceCommissionCommand>(command))return "political_commission_governance";return std::holds_alternative<GovernanceAcceptCommand>(command)?"governance_accept":"governance_cancel";}
Outcome<GovernanceQuote> preview_governance_business(const GovernanceReadContext& c,const StableId& actor,const GovernanceCommand& command){
    if(!c.governance_catalog.enabled)return {false,{},common_failure("GovernanceNotEnabled",{},ErrorKind::unsupported_capability).error};
    auto valid=std::visit([&](const auto& value)->Status{using T=std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T,PoliticalGovernanceCommissionCommand>)return issue_preview(c,actor,value);
        else if constexpr(std::is_same_v<T,GovernanceAcceptCommand>){AuthorityScope scope(c);const auto& access=scope.get();auto quote=preview_governance_acceptance(c.common,c.governance_tasks,access,actor,value.authority_fact_id,value.expected_world_revision);if(!quote.ok)return {false,{},quote.error};return preview_public_relief_resources(c.common,c.economy,c.relief_catalog,c.relief,access,value.authority_fact_id,actor,value.expected_world_revision);}
        else {if(value.expected_world_revision!=c.common.revision())return common_failure("StaleWorldRevision");
            // 公共命令先核本人实际接受源，外人不能按内部错误码探测任务存在与否。
            const auto* known=c.governance_tasks.governance_contracts.find(value.task_id);if(!known||known->acceptance.executor_actor_id!=actor)return common_failure("TaskNotKnown",value.task_id);
            auto quote=preview_governance_cancellation(c.common,c.tasks,c.governance_tasks,actor,value.task_id);if(quote.ok&&quote.value.completion)return common_failure("TaskNotActive",value.task_id);return {quote.ok,{},quote.error};}
    },command);if(!valid.ok)return {false,{},valid.error};return {true,{c.common.state().identity,actor,command,0},{}};
}
Outcome<GovernanceEffects> apply_governance_business(GovernanceWriteContext& c,const StableId& actor,const GovernanceCommand& command,const GovernanceQuote& quote){
    if(!same_identity(c.common.state().identity,quote.identity)||quote.actor_id!=actor||quote.duration!=0||governance_command_kind(command)!=governance_command_kind(quote.command)||!compare_boundary(encode_governance_command(command),encode_governance_command(quote.command)).exact())return {false,{},common_failure("StaleGovernanceQuote").error};
    auto prepared=preview_governance_business(c.read(),actor,command);if(!prepared.ok)return {false,{},prepared.error};AuthorityScope scope(c.read());const auto& access=scope.get();
    return std::visit([&](const auto& value)->Outcome<GovernanceEffects>{using T=std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T,PoliticalGovernanceCommissionCommand>){auto issued=issue_governance_authority_in_transaction(c.common,c.common_catalog,c.politics,c.governance,c.governance_catalog,c.organization,c.organization_catalog,c.activity,actor,value);if(!issued.ok)return {false,{},issued.error};return {true,{issued.value.authority_fact_id,{},{},issued.value.authority_fact_id,{}},{}};}
        else if constexpr(std::is_same_v<T,GovernanceAcceptCommand>){
            auto accepted=begin_governance_task(c.common,c.tasks,c.governance_tasks,c.common_catalog,access,actor,value.authority_fact_id,value.expected_world_revision,[&](const TaskGovernanceRead& tasks,const GovernanceTaskAcceptance& accepted,const GovernanceAuthorityApproval&)->Outcome<PublicReliefTerms>{return reserve_public_relief_in_candidate(c.common,c.economy,c.relief_catalog,c.relief,tasks,access,accepted.task_id);});
            if(!accepted.ok)return {false,{},accepted.error};const auto* contract=c.governance_tasks.governance_contracts.find(accepted.value.task_id);if(!contract)return {false,{},common_failure("MissingGovernanceContract",accepted.value.task_id,ErrorKind::internal).error};
            return {true,{value.authority_fact_id,accepted.value.task_id,contract->action_id,accepted.value.fact_id,accepted.value.relief_terms_id},{}};
        }else{
            GovernanceCancellationServices services;services.release_action=c.release_action;services.release_resources=[&](const TaskGovernanceRead& tasks,const GovernanceTaskSource& task){return release_public_relief_in_candidate(c.common,c.relief,tasks,task.acceptance.task_id);};
            auto cancelled=cancel_governance_task(c.common,c.tasks,c.governance_tasks,actor,value.task_id,services);if(!cancelled.ok)return {false,{},cancelled.error};const auto* contract=c.governance_tasks.governance_contracts.find(value.task_id);if(!contract)return {false,{},common_failure("MissingGovernanceContract",value.task_id,ErrorKind::internal).error};
            StableId released;for(const auto& row:c.relief.releases.rows())if(row.second.terms_id==contract->acceptance.relief_terms_id){if(!released.empty())return {false,{},common_failure("DuplicatePublicReliefRelease",value.task_id,ErrorKind::internal).error};released=row.first;}
            return {true,{contract->acceptance.authority_source_fact_id,value.task_id,contract->action_id,cancelled.value.fact_id,released},{}};
        }
    },command);
}
Status validate_governance_receipt_source(const GovernanceReadContext& c,const StableId& actor,const StableId& kind,Integer tick,const GovernanceEffects& effects){
    AuthorityScope scope(c);const auto& access=scope.get();auto approved=access.governance_authority_approval(effects.authority_fact_id);if(!approved.ok)return {false,{},approved.error};
    if(kind=="political_commission_governance")return effects.authority_fact_id.rfind("politics_governance_authority:",0)==0&&approved.value.issuer_actor_id==actor&&approved.value.issued_tick==tick&&effects.result_fact_id==effects.authority_fact_id&&effects.task_id.empty()&&effects.action_id.empty()&&effects.relief_fact_id.empty()?common_success():common_failure("GovernanceReceiptSourceMismatch",effects.result_fact_id);
    GovernanceTaskRead read(c.common,c.tasks,c.governance_tasks);auto task=read.governance_task_source(effects.task_id);if(!task.ok)return {false,{},task.error};const auto* contract=c.governance_tasks.governance_contracts.find(effects.task_id);const auto& accepted=task.value.acceptance;
    if(!contract||contract->action_id!=effects.action_id||accepted.authority_source_fact_id!=effects.authority_fact_id||accepted.executor_actor_id!=actor)return common_failure("GovernanceReceiptSourceMismatch",effects.result_fact_id);
    if(kind=="governance_accept")return accepted.fact_id==effects.result_fact_id&&accepted.accepted_tick==tick&&accepted.relief_terms_id==effects.relief_fact_id&&c.relief.commitments.find(effects.relief_fact_id)?common_success():common_failure("GovernanceReceiptSourceMismatch",effects.result_fact_id);
    if(kind!="governance_cancel"||!task.value.completion||task.value.completion->result!=GovernanceTaskResult::cancelled||task.value.completion->fact_id!=effects.result_fact_id||task.value.completion->completed_tick!=tick)return common_failure("GovernanceReceiptSourceMismatch",effects.result_fact_id);
    const auto* release=c.relief.releases.find(effects.relief_fact_id);return release&&release->terms_id==accepted.relief_terms_id&&release->tick==tick?common_success():common_failure("GovernanceReceiptSourceMismatch",effects.relief_fact_id);
}
}

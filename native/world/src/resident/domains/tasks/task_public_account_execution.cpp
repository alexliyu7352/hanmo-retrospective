// 逐原角色核对同ID执行账户；不得合并预算退款与报酬退款，也不得把托管货权转给新组织。
#include "task_public_account_execution.hpp"
#include "task_public_account_phase.hpp"
#include "resident/domains/politics/takeover/politics_takeover_contract.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
bool same_subject(const SubjectRef& a,const SubjectRef& b){return a.kind==b.kind&&a.id==b.id;}
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool contains(const std::vector<StableId>& values,const StableId& id){return std::find(values.begin(),values.end(),id)!=values.end();}
StableId role(TaskAccountPurpose purpose){switch(purpose){
    case TaskAccountPurpose::funding:return "funding";case TaskAccountPurpose::reward:return "reward";
    case TaskAccountPurpose::payee:return "payee";case TaskAccountPurpose::escrow:return "escrow";
    case TaskAccountPurpose::delivery:return "delivery";case TaskAccountPurpose::return_custody:return "return_custody";
    case TaskAccountPurpose::shipment_source:return "shipment_source";case TaskAccountPurpose::shipment_cargo:return "shipment_cargo";
    case TaskAccountPurpose::shipment_destination:return "shipment_destination";}return {};
}
Outcome<TaskPublicExecution> binding(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const StableId& id){
    TaskPublicExecution out;out.responsibility_id=id;const auto* contract=tasks.contracts.find(id);
    const auto* task=contract&&contract->current_task_id?tasks.tasks.find(*contract->current_task_id):tasks.tasks.find(id);
    if(!task||task->id.empty()||(!contract&&task->contract_id)||(contract&&(contract->id!=id||task->contract_id!=id)))return {false,{},common_failure("UnknownTaskResponsibility",id).error};
    const auto* definition=resolve_task_definition(tasks,catalog,*task);if(!definition)return {false,{},common_failure("UnknownOffer",task->offer_id).error};
    const auto* actor=common.actor(task->assignee);if(!actor)return {false,{},common_failure("UnknownTaskAssignee",task->id).error};
    out.original_issuer=contract?contract->issuer:definition->issuer;out.original_assignee=contract?contract->assignee:SubjectRef{SubjectKind::actor,task->assignee};
    if(out.original_issuer.kind!=Owner::Kind::actor&&out.original_issuer.kind!=Owner::Kind::organization)return {false,{},common_failure("InvalidTaskIssuer",id).error};
    if(out.original_assignee.kind!=SubjectKind::actor&&out.original_assignee.kind!=SubjectKind::organization)return {false,{},common_failure("UnsupportedTaskAssignee",id).error};
    const auto add=[&](TaskAccountPurpose p,const StableId& container,bool inventory,const Owner& owner){out.accounts.push_back({p,container,inventory,false,owner});};
    add(TaskAccountPurpose::funding,contract?contract->frozen_terms.source_wallet_id:definition->funding_wallet,false,out.original_issuer);
    add(TaskAccountPurpose::reward,contract?contract->frozen_terms.reward_wallet_id:definition->funding_wallet,false,out.original_issuer);
    const Owner payee{out.original_assignee.kind==SubjectKind::actor?Owner::Kind::actor:Owner::Kind::organization,out.original_assignee.id,{}};
    add(TaskAccountPurpose::payee,contract?contract->frozen_terms.payee_wallet_id:actor->wallet_id,false,payee);
    std::vector<StableId> task_ids=contract?contract->historical_task_ids:std::vector<StableId>{};if(!contains(task_ids,task->id))task_ids.push_back(task->id);
    for(const auto& task_id:task_ids){const auto* source=tasks.tasks.find(task_id);if(!source||(contract&&source->contract_id!=id))return {false,{},common_failure("TaskContractBindingMismatch",task_id).error};add(TaskAccountPurpose::escrow,source->budget_wallet_id,false,Owner{Owner::Kind::task,task_id,{}});}
    if(contract){out.original_delivery_beneficiary=contract->frozen_terms.beneficiary;add(TaskAccountPurpose::delivery,contract->frozen_terms.target_inventory_id,true,out.original_issuer);}
    else{StableId destination;if(out.original_issuer.kind==Owner::Kind::organization)destination="store:"+out.original_issuer.id;else{const auto* issuer=common.actor(out.original_issuer.id);if(!issuer)return {false,{},common_failure("UnknownTaskIssuer",id).error};destination=issuer->inventory_id;}add(TaskAccountPurpose::return_custody,destination,true,out.original_issuer);
        // 静态委托同样保留真实货物受益人；收件人或护送对象不能被公库接任者替代。
        if(definition->template_id=="procurement"){out.original_delivery_beneficiary=SubjectRef{out.original_issuer.kind==Owner::Kind::actor?SubjectKind::actor:SubjectKind::organization,out.original_issuer.id};add(TaskAccountPurpose::delivery,destination,true,out.original_issuer);}
        else if(definition->template_id=="message"){const auto* receiver=common.actor(definition->goal.receiver_actor);if(!receiver)return {false,{},common_failure("MissingTaskMessageRecipient",id).error};out.original_delivery_beneficiary=SubjectRef{SubjectKind::actor,receiver->id};add(TaskAccountPurpose::delivery,receiver->inventory_id,true,Owner{Owner::Kind::actor,receiver->id,{}});}
        else if(definition->template_id=="escort")out.original_delivery_beneficiary=SubjectRef{SubjectKind::actor,definition->goal.actor_id};
    }
    // Shipment缺可证明的创建先后时，原历史reader已明确拒绝接管登记；这里也不补一套推断。
    for(const auto& row:tasks.shipments.rows())if(row.second.cargo_owner.kind==Owner::Kind::task&&contains(task_ids,row.second.cargo_owner.id))return {false,{},common_failure("TaskShipmentCreationSourceRequired",row.first).error};
    return {true,std::move(out),{}};
}
Outcome<TaskPublicExecution> apply_source(const CommonStore& common,TaskPublicExecution out,const std::optional<PoliticalResponsibilityExecution>& source,bool historical,Integer observed_tick){
    if(!source)return {true,std::move(out),{}};const auto& s=*source;const auto& r=s.registration;
    if(s.responsibility.kind!=PoliticalResponsibilityKind::task_contract||s.responsibility.responsibility_id!=out.responsibility_id||s.responsibility.unit_id||s.responsibility.asset_id||s.registration_fact_id.empty()||s.registration_fact_id!=r.registration_fact_id||r.occupation_fact_id.empty()||r.former_organization_id.empty()||r.successor_organization_id.empty()||r.former_organization_id==r.successor_organization_id||r.occupation_tick<0||r.occupation_tick>observed_tick||!same_identity(r.source_identity,common.state().identity))return {false,{},common_failure("TaskPoliticalSourceMismatch",out.responsibility_id).error};
    out.registration_fact_id=s.registration_fact_id;out.code=s.code;out.may_progress=false;out.may_settle=false;
    for(auto& account:out.accounts){account.affected=contains(account.inventory?r.public_inventory_ids:r.public_wallet_ids,account.container_id);if(account.affected)account.execution_owner={Owner::Kind::organization,r.successor_organization_id,{}};}
    if(s.status==PoliticalResponsibilityExecutionStatus::pending){if(s.decision)return {false,{},common_failure("TaskPoliticalPendingDecisionMismatch").error};return {true,std::move(out),{}};}
    if(s.status!=PoliticalResponsibilityExecutionStatus::approved&&s.status!=PoliticalResponsibilityExecutionStatus::suspended)return {false,{},common_failure("UnknownPoliticalResponsibilityStatus").error};
    if(!s.decision)return {false,{},common_failure("MissingTaskPoliticalDecision").error};const auto& d=*s.decision;
    const SubjectRef principal{out.original_issuer.kind==Owner::Kind::actor?SubjectKind::actor:SubjectKind::organization,out.original_issuer.id};
    if(d.decision_fact_id.empty()||d.takeover_fact_id.empty()||d.responsibility.kind!=s.responsibility.kind||d.responsibility.responsibility_id!=out.responsibility_id||d.responsibility.unit_id||d.responsibility.asset_id||!d.original_principal||!same_subject(*d.original_principal,principal)||!d.original_assignee||!same_subject(*d.original_assignee,out.original_assignee)||!d.original_beneficiaries.empty()||!d.collection_beneficiaries.empty()||!d.original_assets.empty()||d.payment_wallet_id||d.receipt_wallet_id||d.payment_inventory_id||d.receipt_inventory_id||!d.production_asset_ids.empty()||d.original_delivery_beneficiaries.size()!=(out.original_delivery_beneficiary?1u:0u)||(out.original_delivery_beneficiary&&!same_subject(d.original_delivery_beneficiaries.front(),*out.original_delivery_beneficiary)))return {false,{},common_failure("TaskPoliticalDecisionBindingMismatch",out.responsibility_id).error};
    out.decision_fact_id=d.decision_fact_id;
    if(d.disposition==PoliticalResponsibilityDisposition::suspend||d.disposition==PoliticalResponsibilityDisposition::settlement_required){
        if(s.status!=PoliticalResponsibilityExecutionStatus::suspended||d.execution_principal||!d.execution_accounts.empty())return {false,{},common_failure("TaskPoliticalDispositionMismatch").error};
        return {true,std::move(out),{}};
    }
    if(d.disposition==PoliticalResponsibilityDisposition::continue_under_original){
        if(!d.execution_principal||!same_subject(*d.execution_principal,principal)||!d.execution_accounts.empty())return {false,{},common_failure("TaskPoliticalOriginalExecutionMismatch").error};
        // 重占领的former只描述上一个公库Owner；原委托主体始终来自Task冻结责任。
        for(auto& account:out.accounts)if(account.affected)account.execution_owner=out.original_issuer;
    }else if(d.disposition==PoliticalResponsibilityDisposition::assume_by_successor){
        if(!d.execution_principal||d.execution_principal->kind!=SubjectKind::organization||d.execution_principal->id!=r.successor_organization_id)return {false,{},common_failure("TaskPoliticalExecutionPrincipalMismatch").error};
        std::set<std::tuple<StableId,StableId,bool>> expected,actual;
        for(const auto& account:out.accounts)if(account.affected){
            if(account.inventory|| (account.purpose!=TaskAccountPurpose::funding&&account.purpose!=TaskAccountPurpose::reward))return {false,{},common_failure("TaskCustodyDispositionRequired",account.container_id).error};
            expected.emplace(role(account.purpose),account.container_id,false);
        }
        if(expected.empty())return {false,{},common_failure("TaskPaymentAccountTakeoverRequired").error};
        for(const auto& account:d.execution_accounts){if(account.original_container_id!=account.execution_container_id||!actual.emplace(account.role,account.original_container_id,account.inventory).second)return {false,{},common_failure("TaskExecutionAccountMismatch").error};}
        if(expected!=actual)return {false,{},common_failure("TaskExecutionAccountCoverageMismatch").error};
    }else return {false,{},common_failure("TaskPoliticalDispositionMismatch").error};
    // 合法批准可因控制权/组织状态变化暂停；保留原决定，不伪造撤销或当作损坏来源。
    if(s.status==PoliticalResponsibilityExecutionStatus::suspended){if(s.code.empty())return {false,{},common_failure("MissingTaskPoliticalSuspensionReason").error};return {true,std::move(out),{}};}
    out.may_progress=true;out.may_settle=true;out.code.clear();
    if(!historical){auto valid=validate_task_public_account_execution_assets(common,out);if(!valid.ok)return {false,{},valid.error};}
    return {true,std::move(out),{}};
}
}
Outcome<TaskPublicExecution> read_task_public_account_execution(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const PoliticalTakeoverRead* authority,const StableId& id,const DynamicStepInput* phase){
    auto observed=task_public_account_observed_tick(common,phase);if(!observed.ok)return {false,{},observed.error};
    auto original=binding(common,tasks,catalog,id);if(!original.ok)return original;
    if(!authority){auto valid=validate_task_public_account_execution_assets(common,original.value);return valid.ok?std::move(original):Outcome<TaskPublicExecution>{false,{},valid.error};}
    auto source=authority->find_political_responsibility_for_execution({PoliticalResponsibilityKind::task_contract,id,{},{}},common.state().identity);if(!source.ok)return {false,{},source.error};
    auto result=apply_source(common,std::move(original.value),source.value,false,observed.value);if(!result.ok)return result;
    // 暂停读不授权任何写入；变更后的资产由完整World军事/政治链核验，不能在此补造新Owner。
    if(!result.value.may_progress&&!result.value.may_settle)return result;
    auto valid=validate_task_public_account_execution_assets(common,result.value);return valid.ok?std::move(result):Outcome<TaskPublicExecution>{false,{},valid.error};
}
Status validate_task_public_account_execution_assets(const CommonStore& common,const TaskPublicExecution& source){
    for(const auto& account:source.accounts){if(account.container_id.empty())return common_failure("MissingTaskResponsibilityAccount");const Owner* owner=nullptr;if(account.inventory){const auto* v=common.inventory(account.container_id);if(v)owner=&v->owner;}else{const auto* v=common.wallet(account.container_id);if(v)owner=&v->owner;}if(!owner||!owner_equal(*owner,account.execution_owner))return common_failure("TaskExecutionAccountOwnerMismatch",account.container_id);}
    return common_success();
}
Outcome<TaskPublicExecution> read_task_public_account_payment_authority(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const PoliticalTakeoverRead& authority,const TaskContractPaymentSource& payment,const DynamicStepInput* phase){
    auto observed=task_public_account_observed_tick(common,phase);if(!observed.ok)return {false,{},observed.error};
    const auto* header=common.state().facts.find(payment.fact_id);
    if(!header||header->id!=payment.fact_id||header->domain!="tasks"||header->kind!=task_contract_payment_fact_kind||payment.tick<0||payment.tick>observed.value||payment.legs.empty())return {false,{},common_failure("InvalidTaskContractPaymentSource",payment.fact_id).error};
    auto original=binding(common,tasks,catalog,payment.responsibility_id);if(!original.ok)return original;
    const auto* task=tasks.tasks.find(payment.task_id);
    const auto* contract=tasks.contracts.find(payment.responsibility_id);
    if(!task||(contract?(task->contract_id!=contract->id||(!contains(contract->historical_task_ids,task->id)&&contract->current_task_id!=task->id)):(task->contract_id||task->id!=payment.responsibility_id)))return {false,{},common_failure("TaskPaymentBindingMismatch",payment.fact_id).error};
    std::set<TaskContractPaymentPurpose> seen;Integer total=0;
    for(const auto& leg:payment.legs){
        TaskAccountPurpose destination;
        switch(leg.purpose){case TaskContractPaymentPurpose::budget_return:destination=TaskAccountPurpose::funding;break;case TaskContractPaymentPurpose::reward_payment:destination=TaskAccountPurpose::payee;break;case TaskContractPaymentPurpose::reward_return:destination=TaskAccountPurpose::reward;break;default:return {false,{},common_failure("UnknownTaskContractPaymentPurpose",payment.fact_id).error};}
        const auto account=std::find_if(original.value.accounts.begin(),original.value.accounts.end(),[&](const auto& value){return value.purpose==destination&&!value.inventory;});
        if(account==original.value.accounts.end()||leg.from_wallet_id!=task->budget_wallet_id||leg.to_wallet_id!=account->container_id||leg.from_wallet_id==leg.to_wallet_id||leg.amount<=0||!seen.insert(leg.purpose).second||total>std::numeric_limits<Integer>::max()-leg.amount)return {false,{},common_failure("TaskPaymentLegMismatch",payment.fact_id).error};
        total+=leg.amount;
    }
    auto source=authority.political_responsibility_before_fact({PoliticalResponsibilityKind::task_contract,payment.responsibility_id,{},{}},payment.fact_id,payment.tick,common.state().identity);if(!source.ok)return {false,{},source.error};
    auto result=apply_source(common,std::move(original.value),source.value,true,observed.value);if(!result.ok)return result;
    if(!result.value.may_settle||result.value.registration_fact_id!=payment.registration_fact_id||result.value.decision_fact_id!=payment.decision_fact_id)return {false,{},common_failure("TaskPaymentWithoutPoliticalAuthority",payment.fact_id).error};
    return result;
}
Outcome<TaskPublicExecution> read_task_static_payment_authority(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const PoliticalTakeoverRead& authority,const TaskStaticPaymentSource& payment,const DynamicStepInput* phase){
    auto observed=task_public_account_observed_tick(common,phase);if(!observed.ok)return {false,{},observed.error};
    const auto* header=common.state().facts.find(payment.fact_id);const auto* task=tasks.tasks.find(payment.task_id);
    if(!task||task->contract_id||!catalog.offers.find(task->offer_id)||!header||header->id!=payment.fact_id||header->domain!="tasks"||header->kind!=task_static_payment_fact_kind||payment.tick<0||payment.tick>observed.value)return {false,{},common_failure("InvalidTaskStaticPaymentSource",payment.fact_id).error};
    auto original=binding(common,tasks,catalog,payment.task_id);if(!original.ok)return original;
    auto source=authority.political_responsibility_before_fact({PoliticalResponsibilityKind::task_contract,payment.task_id,{},{}},payment.fact_id,payment.tick,common.state().identity);if(!source.ok)return {false,{},source.error};
    auto result=apply_source(common,std::move(original.value),source.value,true,observed.value);if(!result.ok)return result;
    if(!result.value.may_settle||result.value.registration_fact_id!=payment.registration_fact_id||result.value.decision_fact_id!=payment.decision_fact_id)return {false,{},common_failure("TaskStaticPaymentWithoutPoliticalAuthority",payment.fact_id).error};return result;
}
}

#include "shop_orders/task_shop_order_sources.hpp"
// 接管只读取该次选中资产真正涉及的原任务，未有政治处置源前不授权付款或改派。
#include "task_public_account_dependencies.hpp"
#include "task_public_account_phase.hpp"
#include "task_static_public_payment.hpp"
#include "governance_task_state.hpp"
#include "envoy_task_state.hpp"
#include "resident/domains/military/siege_model.hpp"
#include "resident/domains/military/occupation_model.hpp"
#include <charconv>
#include <algorithm>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
// 顺序保原账，去重只用于同一责任的重复引用，不能合并两份合同。
void append_once(std::vector<StableId>& ids,const StableId& id){if(std::find(ids.begin(),ids.end(),id)==ids.end())ids.push_back(id);}
bool chosen(const std::vector<StableId>& ids,const StableId& id){return std::find(ids.begin(),ids.end(),id)!=ids.end();}
bool affected(const TaskPublicAccountDependency& value){
    for(const auto& ref:value.wallets)if(ref.selected)return true;
    for(const auto& ref:value.inventories)if(ref.selected)return true;
    for(const auto& ref:value.cargo)if(ref.selected_container)return true;
    return false;
}
}
// 此处保留历史原主体，资产Owner变化绝不倒改contract.issuer或冻结条款。
Outcome<TaskPublicAccountDependencies> read_task_public_account_dependencies(
    const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,
    const GovernanceTaskState* governance,const EnvoyTaskState* envoy,const TaskPublicAccountSelection& selection){
    using Result=Outcome<TaskPublicAccountDependencies>;
    const auto fail=[](const StableId& code,const StableId& id=StableId{})->Result{return {false,{},common_failure(code,id).error};};
    if(selection.original_owner.kind!=Owner::Kind::organization||selection.original_owner.id.empty())return fail("TaskTakeoverRequiresPublicOwner");
    std::set<StableId> seen;
    for(const auto& id:selection.wallet_ids){const auto* wallet=common.wallet(id);if(id.empty()||!seen.insert(id).second||!wallet||!owner_equal(wallet->owner,selection.original_owner))return fail("InvalidTaskTakeoverWallet",id);}
    seen.clear();
    for(const auto& id:selection.inventory_ids){const auto* inventory=common.inventory(id);if(id.empty()||!seen.insert(id).second||!inventory||!owner_equal(inventory->owner,selection.original_owner))return fail("InvalidTaskTakeoverInventory",id);}
    if(governance&&governance->version!=1)return fail("UnsupportedGovernanceTaskVersion");
    if(envoy&&envoy->version!=1&&envoy->version!=2)return fail("UnsupportedEnvoyTaskVersion");
    if(tasks.shop_orders){auto valid=validate_task_shop_order_acceptances(common,tasks,*tasks.shop_orders);if(!valid.ok)return {false,{},valid.error};}TaskPublicAccountDependencies out;out.source_identity=common.state().identity;
    std::map<StableId,std::size_t> by_task;
    auto add_wallet=[&](TaskPublicAccountDependency& value,TaskAccountPurpose purpose,const StableId& id)->bool{
        if(id.empty()||!common.wallet(id))return false;
        value.wallets.push_back({purpose,id,chosen(selection.wallet_ids,id)});return true;
    };
    auto add_inventory=[&](TaskPublicAccountDependency& value,TaskAccountPurpose purpose,const StableId& id)->bool{
        if(id.empty()||!common.inventory(id))return false;
        value.inventories.push_back({purpose,id,chosen(selection.inventory_ids,id)});return true;
    };
    // 运行合同含真正付款与受益主体；实际任务可能有历史实例，不能只看当前一条。
    for(const auto& row:tasks.contracts.rows()){
        const auto& contract=row.second;const auto& terms=contract.frozen_terms;
        if(contract.id!=row.first||contract.id.empty()||!contract.current_task_id)return fail("InvalidTaskContract",row.first);
        TaskPublicAccountDependency value;value.responsibility_id=contract.id;value.contract_id=contract.id;
        value.original_issuer=contract.issuer;value.assignee=contract.assignee;value.delivery_beneficiary=terms.beneficiary;
        value.contract_status=contract.status;value.settlement_id=contract.settlement_id;
        value.execution_stopped=contract.execution_stopped;value.stop_reason=contract.stop_reason;
        value.task_ids=contract.historical_task_ids;append_once(value.task_ids,*contract.current_task_id);
        seen.clear();
        for(const auto& task_id:value.task_ids){const auto* task=tasks.tasks.find(task_id);
            if(!seen.insert(task_id).second||!task||task->id!=task_id||!task->contract_id||*task->contract_id!=contract.id||by_task.count(task_id))return fail("TaskContractBindingMismatch",task_id);
            if(!add_wallet(value,TaskAccountPurpose::escrow,task->budget_wallet_id))return fail("MissingTaskEscrow",task_id);
            const auto* escrow=common.wallet(task->budget_wallet_id);if(escrow->owner.kind!=Owner::Kind::task||escrow->owner.id!=task_id)return fail("TaskEscrowOwnerMismatch",task_id);
            value.task_statuses.emplace_back(task_id,task->status);by_task.emplace(task_id,out.responsibilities.size());
        }
        if(!add_wallet(value,TaskAccountPurpose::funding,terms.source_wallet_id)||!add_wallet(value,TaskAccountPurpose::reward,terms.reward_wallet_id)||!add_wallet(value,TaskAccountPurpose::payee,terms.payee_wallet_id))return fail("MissingTaskContractWallet",contract.id);
        if(!add_inventory(value,TaskAccountPurpose::delivery,terms.target_inventory_id))return fail("MissingTaskContractInventory",contract.id);
        out.responsibilities.push_back(std::move(value));
    }
    // 已登记的非采购Task只经真实sidecar绑定分流，不能吞掉普通UnknownOffer。
    for(const auto& row:tasks.tasks.rows()){
        const auto& task=row.second;if(by_task.count(row.first))continue;
        if(task.id!=row.first||task.contract_id)return fail("UnboundTaskContract",row.first);
        const auto* gov=governance?governance->governance_contracts.find(task.id):nullptr;
        const auto* mission=envoy?envoy->envoy_contracts.find(task.id):nullptr;
        if(gov&&mission)return fail("DuplicateTaskResponsibility",task.id);if(tasks.shop_orders&&tasks.shop_orders->by_task.count(task.id)){if(gov||mission)return fail("DuplicateTaskResponsibility",task.id);const auto* b=tasks.shop_orders->bindings.find(tasks.shop_orders->by_task.at(task.id));if(!b)return fail("TaskShopOrderIndexMismatch",task.id);if(chosen(selection.wallet_ids,b->terms.shop_wallet_id)||chosen(selection.wallet_ids,b->terms.client_wallet_id)||chosen(selection.inventory_ids,b->terms.stock_inventory_id)||chosen(selection.inventory_ids,b->terms.client_inventory_id))return fail("TaskShopOrderPrivateAssetsRequired",task.id);continue;}
        if(gov){const auto& source=gov->acceptance;if(source.task_id!=task.id||source.executor_actor_id!=task.assignee||source.authority_source_fact_id!=task.offer_id||!task.budget_wallet_id.empty())return fail("GovernanceTaskBindingMismatch",task.id);continue;}
        if(mission){const auto& source=mission->acceptance;if(source.task_id!=task.id||source.envoy_actor_id!=task.assignee||source.proposal_id!=task.offer_id||!task.budget_wallet_id.empty())return fail("EnvoyTaskBindingMismatch",task.id);continue;}
        const auto* offer=resolve_task_definition(tasks,catalog,task);if(!offer)return fail("UnknownOffer",task.offer_id);
        const auto* actor=common.actor(task.assignee);if(!actor)return fail("UnknownActor",task.assignee);
        TaskPublicAccountDependency value;value.responsibility_id=task.id;value.original_issuer=offer->issuer;value.assignee={SubjectKind::actor,task.assignee};
        auto static_cash=task_static_payment_responsibility_supported(tasks,catalog,task.id);if(!static_cash.ok)return {false,{},static_cash.error};value.static_cash_execution_supported=static_cash.value;
        value.task_ids.push_back(task.id);value.task_statuses.emplace_back(task.id,task.status);if(!task.settlement_id.empty())value.settlement_id=task.settlement_id;
        if(!add_wallet(value,TaskAccountPurpose::funding,offer->funding_wallet)||!add_wallet(value,TaskAccountPurpose::reward,offer->funding_wallet)||!add_wallet(value,TaskAccountPurpose::payee,actor->wallet_id)||!add_wallet(value,TaskAccountPurpose::escrow,task.budget_wallet_id))return fail("MissingTaskWallet",task.id);
        const auto* escrow=common.wallet(task.budget_wallet_id);if(escrow->owner.kind!=Owner::Kind::task||escrow->owner.id!=task.id)return fail("TaskEscrowOwnerMismatch",task.id);
        StableId destination;
        if(offer->issuer.kind==Owner::Kind::organization)destination="store:"+offer->issuer.id;
        else if(offer->issuer.kind==Owner::Kind::actor){const auto* issuer=common.actor(offer->issuer.id);if(!issuer)return fail("UnknownTaskIssuer",task.id);destination=issuer->inventory_id;}
        else return fail("InvalidTaskIssuer",task.id);
        if(!add_inventory(value,TaskAccountPurpose::return_custody,destination))return fail("MissingTaskReturnInventory",task.id);
        // 私信实际收件人与取消返还人不同，不能把两个账户责任混成一个destination。
        if(offer->template_id=="procurement"){
            if(!add_inventory(value,TaskAccountPurpose::delivery,destination))return fail("MissingTaskDeliveryInventory",task.id);
            value.delivery_beneficiary=SubjectRef{offer->issuer.kind==Owner::Kind::actor?SubjectKind::actor:SubjectKind::organization,offer->issuer.id};
        }else if(offer->template_id=="message"){
            const auto* receiver=common.actor(offer->goal.receiver_actor);if(!receiver||!add_inventory(value,TaskAccountPurpose::delivery,receiver->inventory_id))return fail("MissingTaskMessageRecipient",task.id);
            value.delivery_beneficiary=SubjectRef{SubjectKind::actor,offer->goal.receiver_actor};
        }else if(offer->template_id=="escort")value.delivery_beneficiary=SubjectRef{SubjectKind::actor,offer->goal.actor_id};
        by_task.emplace(task.id,out.responsibilities.size());out.responsibilities.push_back(std::move(value));
    }
    for(const auto& row:tasks.actions.rows()){
        const auto found=by_task.find(row.second.task_id);const auto* action=common.action(row.first);
        if(found==by_task.end()||!action)return fail("InvalidTaskAction",row.first);
        append_once(out.responsibilities[found->second].action_ids,row.first);
    }
    for(const auto& row:tasks.shipments.rows()){
        const auto& shipment=row.second;const auto found=by_task.find(shipment.cargo_owner.id);
        if(shipment.cargo_owner.kind!=Owner::Kind::task||found==by_task.end())return fail("InvalidTaskShipment",row.first);
        auto& value=out.responsibilities[found->second];append_once(value.shipment_ids,row.first);
        if((!shipment.source_inventory_id.empty()&&!add_inventory(value,TaskAccountPurpose::shipment_source,shipment.source_inventory_id))||!add_inventory(value,TaskAccountPurpose::shipment_cargo,shipment.cargo_inventory_id)||!add_inventory(value,TaskAccountPurpose::shipment_destination,shipment.destination_inventory_id))return fail("MissingTaskShipmentInventory",row.first);
    }
    // 受托物即使寄存在被接管公库也保持Task货权，不能被仓库Owner变化吞掉。
    for(const auto& row:common.state().inventories.rows())for(const auto& lot:row.second.lots)if(lot.owner.kind==Owner::Kind::task){
        const auto found=by_task.find(lot.owner.id);if(found==by_task.end())return fail("UnboundTaskCargo",lot.id);
        out.responsibilities[found->second].cargo.push_back({lot.owner.id,row.first,lot.id,chosen(selection.inventory_ids,row.first)});
    }
    out.responsibilities.erase(std::remove_if(out.responsibilities.begin(),out.responsibilities.end(),[](const auto& value){return !affected(value);}),out.responsibilities.end());
    // 后继政治处置以kind/原ID定位，不能让两份不同来源共用一个模糊责任键。
    seen.clear();for(const auto& value:out.responsibilities)if(!seen.insert(value.responsibility_id).second)return fail("AmbiguousTaskResponsibility",value.responsibility_id);
    return {true,std::move(out),{}};
}
}

// 历史读取不复用接管前的当前Owner筛选：资产可合法转属，原责任和原托管来源仍须保留。
namespace hanmo::resident {
namespace {
// 原生next_id的全局序号决定同一半日内先后；非规范字符串不被宽松解析为历史来源。
Outcome<Integer> instance_sequence(const CommonStore& common,const StableId& id,const StableId& prefix){
    const auto lead=prefix+":";if(id.compare(0,lead.size(),lead)!=0)return {false,{},common_failure("TaskHistoryInstanceSourceRequired",id).error};
    const auto text=id.substr(lead.size());Integer n=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),n);
    if(text.empty()||parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||n<=0||n>=common.state().next_instance_id||std::to_string(n)!=text)return {false,{},common_failure("TaskHistoryInstanceSourceRequired",id).error};
    return {true,n,{}};
}
// 调用方仍须验S2整条军事来源；这里只拒绝错种类、未应用、未来和不一致的清单。
Outcome<Integer> occupation_sequence(const CommonStore& common,const MilitaryOccupationFact& source,const DynamicStepInput* phase){
    auto observed=task_public_account_observed_tick(common,phase);if(!observed.ok)return {false,{},observed.error};
    const auto* fact=common.state().facts.find(source.id);auto sequence=instance_sequence(common,source.id,"military");
    if(!sequence.ok)return sequence;
    if(!fact||fact->id!=source.id||fact->kind!="military_occupation"||fact->domain!="military"||source.phase!="completed"||source.tick<0||source.tick>observed.value||source.assets_applied!=true||!source.asset_manifest||source.from_organization_id.empty()||source.to_organization_id.empty()||source.from_organization_id==source.to_organization_id)return {false,{},common_failure("TaskOccupationHistorySourceRequired",source.id).error};
    std::set<StableId> wallets,inventories;
    const auto owners=[&](const Owner& before,const Owner& after){return before.kind==Owner::Kind::organization&&before.id==source.from_organization_id&&after.kind==Owner::Kind::organization&&after.id==source.to_organization_id;};
    for(const auto& row:source.asset_manifest->wallets)if(row.first.empty()||!wallets.insert(row.first).second||!owners(row.second.from_owner,row.second.to_owner))return {false,{},common_failure("InvalidTaskOccupationManifest",source.id).error};
    for(const auto& row:source.asset_manifest->inventories)if(row.first.empty()||!inventories.insert(row.first).second||!owners(row.second.from_owner,row.second.to_owner))return {false,{},common_failure("InvalidTaskOccupationManifest",source.id).error};
    if(wallets!=std::set<StableId>(source.public_wallet_ids.begin(),source.public_wallet_ids.end())||inventories!=std::set<StableId>(source.public_inventory_ids.begin(),source.public_inventory_ids.end())||wallets.size()!=source.public_wallet_ids.size()||inventories.size()!=source.public_inventory_ids.size())return {false,{},common_failure("InvalidTaskOccupationManifest",source.id).error};
    return sequence;
}
// TaskRecord.serial与原next_id必须同源，排除后来创建或同名替换的任务。
Outcome<bool> task_precedes(const CommonStore& common,const TaskRecord& task,Integer occupation){
    auto sequence=instance_sequence(common,task.id,"task");if(!sequence.ok)return {false,false,sequence.error};
    if(task.serial!=sequence.value)return {false,false,common_failure("TaskHistorySerialMismatch",task.id).error};
    return {true,sequence.value<occupation,{}};
}
}
Outcome<TaskPublicAccountDependency> read_task_public_account_responsibility_history(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const MilitaryOccupationFact& occupation,const StableId& key,const DynamicStepInput* phase){
    using Result=Outcome<TaskPublicAccountDependency>;const auto fail=[](const StableId& code,const StableId& id)->Result{return {false,{},common_failure(code,id).error};};
    auto boundary=occupation_sequence(common,occupation,phase);if(!boundary.ok)return {false,{},boundary.error};
    TaskPublicAccountDependency out;out.responsibility_id=key;
    std::set<StableId> selected_wallets,selected_inventories;
    for(const auto& row:occupation.asset_manifest->wallets)selected_wallets.insert(row.first);
    for(const auto& row:occupation.asset_manifest->inventories)selected_inventories.insert(row.first);
    const auto wallet=[&](TaskAccountPurpose purpose,const StableId& id){if(id.empty()||!common.wallet(id))return false;out.wallets.push_back({purpose,id,selected_wallets.count(id)!=0});return true;};
    const auto inventory=[&](TaskAccountPurpose purpose,const StableId& id){if(id.empty()||!common.inventory(id))return false;out.inventories.push_back({purpose,id,selected_inventories.count(id)!=0});return true;};
    if(const auto* contract=tasks.contracts.find(key)){
        auto creation=instance_sequence(common,key,"contract");if(!creation.ok)return {false,{},creation.error};
        if(creation.value>=boundary.value)return fail("TaskResponsibilityNotInOccupation",key);
        if(contract->id!=key||!contract->current_task_id)return fail("InvalidTaskContract",key);
        out.contract_id=key;out.original_issuer=contract->issuer;out.assignee=contract->assignee;out.delivery_beneficiary=contract->frozen_terms.beneficiary;
        auto ids=contract->historical_task_ids;append_once(ids,*contract->current_task_id);std::set<StableId> unique;
        for(const auto& id:ids){const auto* task=tasks.tasks.find(id);if(!task||task->id!=id||!unique.insert(id).second||!task->contract_id||*task->contract_id!=key)return fail("TaskContractBindingMismatch",id);
            auto existed=task_precedes(common,*task,boundary.value);if(!existed.ok)return {false,{},existed.error};if(!existed.value)continue;
            if(task->accepted_tick>occupation.tick||!wallet(TaskAccountPurpose::escrow,task->budget_wallet_id))return fail("TaskContractHistoryUnavailable",id);out.task_ids.push_back(id);
        }
        if(out.task_ids.empty())return fail("TaskContractHistoryUnavailable",key);
        const auto& terms=contract->frozen_terms;
        if(!wallet(TaskAccountPurpose::funding,terms.source_wallet_id)||!wallet(TaskAccountPurpose::reward,terms.reward_wallet_id)||!wallet(TaskAccountPurpose::payee,terms.payee_wallet_id)||!inventory(TaskAccountPurpose::delivery,terms.target_inventory_id))return fail("TaskContractHistoryUnavailable",key);
    }else{
        const auto* task=tasks.tasks.find(key);if(!task||task->id!=key||task->contract_id)return fail("UnknownTaskResponsibility",key);
        auto existed=task_precedes(common,*task,boundary.value);if(!existed.ok)return {false,{},existed.error};if(!existed.value)return fail("TaskResponsibilityNotInOccupation",key);
        const auto* offer=resolve_task_definition(tasks,catalog,*task);const auto* actor=common.actor(task->assignee);
        if(!offer||!actor||task->accepted_tick>occupation.tick)return fail("TaskResponsibilityHistoryUnavailable",key);
        out.task_ids.push_back(key);out.original_issuer=offer->issuer;out.assignee={SubjectKind::actor,task->assignee};
        auto static_cash=task_static_payment_responsibility_supported(tasks,catalog,key);if(!static_cash.ok)return {false,{},static_cash.error};out.static_cash_execution_supported=static_cash.value;
        if(!wallet(TaskAccountPurpose::funding,offer->funding_wallet)||!wallet(TaskAccountPurpose::reward,offer->funding_wallet)||!wallet(TaskAccountPurpose::payee,actor->wallet_id)||!wallet(TaskAccountPurpose::escrow,task->budget_wallet_id))return fail("TaskResponsibilityHistoryUnavailable",key);
        StableId destination;
        if(offer->issuer.kind==Owner::Kind::organization)destination="store:"+offer->issuer.id;
        else if(offer->issuer.kind==Owner::Kind::actor){const auto* issuer=common.actor(offer->issuer.id);if(!issuer)return fail("UnknownTaskIssuer",key);destination=issuer->inventory_id;}
        else return fail("InvalidTaskIssuer",key);
        if(!inventory(TaskAccountPurpose::return_custody,destination))return fail("TaskResponsibilityHistoryUnavailable",key);
        if(offer->template_id=="procurement"){if(!inventory(TaskAccountPurpose::delivery,destination))return fail("TaskResponsibilityHistoryUnavailable",key);out.delivery_beneficiary=SubjectRef{offer->issuer.kind==Owner::Kind::actor?SubjectKind::actor:SubjectKind::organization,offer->issuer.id};}
        else if(offer->template_id=="message"){const auto* receiver=common.actor(offer->goal.receiver_actor);if(!receiver||!inventory(TaskAccountPurpose::delivery,receiver->inventory_id))return fail("TaskResponsibilityHistoryUnavailable",key);out.delivery_beneficiary=SubjectRef{SubjectKind::actor,receiver->id};}
        else if(offer->template_id=="escort")out.delivery_beneficiary=SubjectRef{SubjectKind::actor,offer->goal.actor_id};
    }
    // Shipment当前没有原创建时点；不借今天的运输记录重写当时完整键集合。
    for(const auto& row:tasks.shipments.rows())if(row.second.cargo_owner.kind==Owner::Kind::task&&chosen(out.task_ids,row.second.cargo_owner.id))return fail("TaskShipmentHistoryUnavailable",row.first);
    std::set<StableId> cargo_ids;
    for(const auto& container:occupation.asset_manifest->inventories)for(const auto& lot:container.second.protected_lots)if(lot.owner.kind==Owner::Kind::task&&chosen(out.task_ids,lot.owner.id)){
        if(lot.id.empty()||!cargo_ids.insert(lot.id).second)return fail("InvalidTaskOccupationCargo",lot.id);
        out.cargo.push_back({lot.owner.id,container.first,lot.id,true});
    }
    if(!affected(out))return fail("TaskResponsibilityNotInOccupation",key);
    return {true,std::move(out),{}};
}
Outcome<std::vector<StableId>> read_task_public_account_responsibility_keys(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const GovernanceTaskState* governance,const EnvoyTaskState* envoy,const MilitaryOccupationFact& occupation,const DynamicStepInput* phase){
    auto boundary=occupation_sequence(common,occupation,phase);if(!boundary.ok)return {false,{},boundary.error};
    if(tasks.shop_orders){auto valid=validate_task_shop_order_ledger(common,tasks,*tasks.shop_orders,phase);if(!valid.ok)return {false,{},valid.error};}
    if(governance&&governance->version!=1)return {false,{},common_failure("UnsupportedGovernanceTaskVersion").error};
    if(envoy&&envoy->version!=1&&envoy->version!=2)return {false,{},common_failure("UnsupportedEnvoyTaskVersion").error};
    std::set<StableId> keys;
    const auto add=[&](const StableId& key)->Status{auto source=read_task_public_account_responsibility_history(common,tasks,catalog,occupation,key,phase);if(!source.ok){if(source.error.original.code=="TaskResponsibilityNotInOccupation")return common_success();return {false,{},source.error};}if(!keys.insert(key).second)return common_failure("AmbiguousTaskResponsibility",key);return common_success();};
    for(const auto& row:tasks.contracts.rows()){if(row.first!=row.second.id)return {false,{},common_failure("InvalidTaskContract",row.first).error};auto valid=add(row.first);if(!valid.ok)return {false,{},valid.error};}
    for(const auto& row:tasks.tasks.rows()){
        const auto& task=row.second;if(task.id!=row.first)return {false,{},common_failure("InvalidTaskResponsibility",row.first).error};
        if(task.contract_id){const auto* contract=tasks.contracts.find(*task.contract_id);if(!contract||(contract->current_task_id!=task.id&&!chosen(contract->historical_task_ids,task.id)))return {false,{},common_failure("TaskContractBindingMismatch",task.id).error};continue;}
        const auto* gov=governance?governance->governance_contracts.find(task.id):nullptr;const auto* mission=envoy?envoy->envoy_contracts.find(task.id):nullptr;
        if(gov&&mission)return {false,{},common_failure("DuplicateTaskResponsibility",task.id).error};if(tasks.shop_orders&&tasks.shop_orders->by_task.count(task.id)){if(gov||mission)return {false,{},common_failure("DuplicateTaskResponsibility",task.id).error};continue;}
        if(gov){const auto& a=gov->acceptance;if(a.task_id!=task.id||a.executor_actor_id!=task.assignee||a.authority_source_fact_id!=task.offer_id||!task.budget_wallet_id.empty())return {false,{},common_failure("GovernanceTaskBindingMismatch",task.id).error};continue;}
        if(mission){const auto& a=mission->acceptance;if(a.task_id!=task.id||a.envoy_actor_id!=task.assignee||a.proposal_id!=task.offer_id||!task.budget_wallet_id.empty())return {false,{},common_failure("EnvoyTaskBindingMismatch",task.id).error};continue;}
        auto valid=add(task.id);if(!valid.ok)return {false,{},valid.error};
    }
    // 原托管lot不得指向当时尚不存在的Task，防止孤立cargo被整个责任表删除后漏验。
    for(const auto& container:occupation.asset_manifest->inventories)for(const auto& lot:container.second.protected_lots)if(lot.owner.kind==Owner::Kind::task){
        const auto* task=tasks.tasks.find(lot.owner.id);if(!task)return {false,{},common_failure("UnboundTaskOccupationCargo",lot.id).error};auto existed=task_precedes(common,*task,boundary.value);if(!existed.ok)return {false,{},existed.error};if(!existed.value)return {false,{},common_failure("InvalidTaskOccupationCargo",lot.id).error};
        const auto key=task->contract_id.value_or(task->id);if(!keys.count(key))return {false,{},common_failure("UnboundTaskOccupationCargo",lot.id).error};
    }
    return {true,std::vector<StableId>(keys.begin(),keys.end()),{}};
}
}

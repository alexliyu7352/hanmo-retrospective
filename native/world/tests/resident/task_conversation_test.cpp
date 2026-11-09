// 只验证真实CommonStore上的当面任务投影与隔离；本门不宣称覆盖跨域结算或GUI。
#include "resident/domains/tasks/task_lifecycle_view.hpp"
#include "resident/domains/tasks/governance_task_rules.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
using V=BoundaryValue;
// 读取测试输出字段，缺字段立即中止而不是用默认值遮掉失败。
const V& field(const V& value,const char* key){auto* found=value.find(key);assert(found);return *found;}
// 两个合法岗位由同一真人办理，另有人持有不可见的私人任务。
int main(){
    auto shared=std::make_shared<CommonCatalog>();shared->places.insert("office",{"office","town",{}});
    for(auto id:{"clerk","post"}){RoleDefinition role;role.id=id;role.place_id="office";role.initial_actor="giver";role.services={"accept_offer","report_offer","deliver_message"};shared->roles.insert(id,role);}
    CommonState seed;seed.identity.tick=10;seed.identity.revision=7;seed.next_instance_id=20;seed.metadata.player_actor_id="player";
    for(auto id:{"player","giver","other"}){ActorRecord actor;actor.id=id;actor.alive=true;actor.available=true;actor.hp=100;actor.location=SettlementLocation{"town","office"};actor.inventory_id="bag:"+actor.id;actor.wallet_id="cash:"+actor.id;seed.actors.insert(id,actor);seed.wallets.insert(actor.wallet_id,{actor.wallet_id,{Owner::Kind::actor,id,{}},100,{}});seed.inventories.insert(actor.inventory_id,{actor.inventory_id,{Owner::Kind::actor,id,{}},100,0,{}});}
    CommonStore common(seed,shared);InteractionCatalog interaction_catalog;interaction_catalog.common=shared;InteractionState interaction;
    InteractionSession session;session.actor_id="player";session.id="meeting";session.phase="choice";session.context={"office","giver","clerk"};interaction.interactions.push_back(session);
    interaction.knowledge.push_back({"player",{{"grain",3,{"prop","board"}},{"letter",4,{"actor","giver"}},{"private",5,{"actor","other"}}}});
    interaction.knowledge.push_back({"other",{{"secret",1,{"actor","other"}}}});
    TaskCatalog catalog;TaskState tasks;
    for(auto id:{"grain","letter","private","secret"}){TaskDefinition offer;offer.id=id;offer.display_name=id;offer.template_id=StableId(id)=="letter"?"message":"procurement";offer.accept_role=StableId(id)=="letter"?"post":"clerk";offer.report_role=offer.accept_role;offer.issuer={Owner::Kind::actor,StableId(id)=="private"?"other":"giver",{}};offer.goal.item_id="rice";offer.goal.quantity=3;offer.goal.receiver_actor="giver";offer.budget=20;offer.reward=8;offer.accept_ticks=1;offer.deadline_ticks=4;catalog.offers.insert(id,offer);}
    TaskRecord letter;letter.id="letter_task";letter.offer_id="letter";letter.assignee="player";letter.status="ready";letter.serial=1;letter.accepted_tick=2;letter.deadline_tick=9;tasks.tasks.insert(letter.id,letter);
    TaskRecord secret=letter;secret.id="secret_task";secret.offer_id="secret";secret.assignee="other";tasks.tasks.insert(secret.id,secret);
    InteractionAccess access;access.resolve_location=[&](const StableId& id){return common.actor(id)->location;};access.battle_occupied=[](const auto&){return false;};access.role=[](const auto&){return std::optional<RoleAssignmentView>{{"giver",true}};};
    std::vector<TaskCommand> requests;
    TaskConversationPreview preview=[&](const TaskCommand& command)->Outcome<TaskQuote>{requests.push_back(command);auto valid=validate_interaction(common,interaction,interaction_catalog,access,"player",command.context,command.kind=="cancel_task"?"report_offer":command.kind);if(!valid.ok)return {false,{},valid.error};if(command.kind=="report_offer")return {false,{},common_failure("IssuerInsufficientFunds").error};TaskQuote quote;quote.context=valid.value;quote.duration=command.kind=="cancel_task"?0:1;return {true,quote,{}};};
    auto view=query_task_conversation(common,tasks,catalog,interaction,interaction_catalog,access,"player",0,preview);assert(view.ok);const auto& rows=*field(view.value,"rows").as<V::Array>();assert(rows.size()==2);assert(*field(rows[0],"id").as<StableId>()=="grain");assert(*field(rows[1],"id").as<StableId>()=="letter");assert(*field(rows[1],"overdue").as<bool>());assert(*field(rows[0],"discovered_tick").as<Integer>()==3);
    const auto& actions=*field(rows[1],"actions").as<V::Array>();bool report=false,cancel=false;for(const auto& a:actions){if(*field(a,"command").as<StableId>()=="report_offer"){report=true;assert(!*field(a,"available").as<bool>());assert(*field(a,"code").as<StableId>()=="IssuerInsufficientFunds");assert(*field(field(field(a,"params"),"context"),"role_id").as<StableId>()=="post");}if(*field(a,"command").as<StableId>()=="cancel_task")cancel=*field(a,"available").as<bool>();}assert(report&&cancel);
    assert(common.tick()==10&&common.revision()==7&&common.state().next_instance_id==20&&common.wallet("cash:player")->balance==100);assert(tasks.tasks.size()==2&&interaction.knowledge.front().offers.front().discovered_tick==3);
    // 未受领的全目录委托与别人的任务都不进入投影；重复读取不增添知识。
    auto again=query_task_conversation(common,tasks,catalog,interaction,interaction_catalog,access,"player",0,preview);assert(again.ok&&compare_boundary(view.value,again.value).exact());assert(interaction.knowledge.front().offers.size()==3);
    // 同一收信人的旧岗位停用，当前合法岗位仍可接信，不能被首个候选遮掉。
    interaction.interactions.front().context.role_id="post";access.role=[](const StableId& role){return std::optional<RoleAssignmentView>{{"giver",role=="post"}};};
    auto replacement=query_task_conversation(common,tasks,catalog,interaction,interaction_catalog,access,"player",0,preview);assert(replacement.ok);bool delivery=false;
    for(const auto& row:*field(replacement.value,"rows").as<V::Array>())if(*field(row,"id").as<StableId>()=="letter")for(const auto& a:*field(row,"actions").as<V::Array>())if(*field(a,"command").as<StableId>()=="deliver_message"){delivery=*field(a,"available").as<bool>();assert(*field(field(field(a,"params"),"context"),"role_id").as<StableId>()=="post");}assert(delivery);
    interaction.interactions.front().context.role_id="clerk";access.role=[](const auto&){return std::optional<RoleAssignmentView>{{"giver",true}};};
    // 经办人缺席时整个现场来源无效，不展示远方的任务错误。
    common.state().actors.find("giver")->location=SettlementLocation{"town","elsewhere"};auto absent=query_task_conversation(common,tasks,catalog,interaction,interaction_catalog,access,"player",0,preview);assert(!absent.ok&&absent.error.original.code=="PersonAbsent");common.state().actors.find("giver")->location=SettlementLocation{"town","office"};
    interaction.interactions.front().phase="action";auto pending=query_task_conversation(common,tasks,catalog,interaction,interaction_catalog,access,"player",0,preview);assert(!pending.ok&&pending.error.original.code=="NoConversation");interaction.interactions.front().phase="choice";
    auto wrong_actor=query_task_conversation(common,tasks,catalog,interaction,interaction_catalog,access,"other",0,preview);assert(!wrong_actor.ok&&wrong_actor.error.original.code=="NoConversation");
    auto bad_cursor=query_task_conversation(common,tasks,catalog,interaction,interaction_catalog,access,"player",99,preview);assert(!bad_cursor.ok&&bad_cursor.error.original.code=="InvalidTaskConversationCursor");
    // 成功结算后晚些时候回访，不能把现在的日期重新判作旧合同逾期或要求重复结算。
    auto* settled=tasks.tasks.find("letter_task");settled->status="success";settled->reported_tick=8;settled->late=false;settled->settlement_id="settlement:letter";
    auto completed=query_task_conversation(common,tasks,catalog,interaction,interaction_catalog,access,"player",0,preview);assert(completed.ok);
    for(const auto& row:*field(completed.value,"rows").as<V::Array>())if(*field(row,"id").as<StableId>()=="letter"){assert(!row.find("deadline_tick")&&!row.find("overdue"));assert(field(row,"actions").as<V::Array>()->empty());assert(row.find("result_text"));}
    settled->status="ready";settled->reported_tick.reset();settled->late.reset();settled->settlement_id.clear();
    // 未拨款就中断受领不算取消已有承诺，仍按原重受领规则提供办理。
    TaskRecord interrupted;interrupted.id="interrupted";interrupted.offer_id="grain";interrupted.assignee="player";interrupted.status="cancelled";interrupted.budget_wallet_id="budget:interrupted";interrupted.serial=2;tasks.tasks.insert(interrupted.id,interrupted);common.state().wallets.insert(interrupted.budget_wallet_id,{interrupted.budget_wallet_id,{Owner::Kind::task,interrupted.id,{}},0,{}});
    auto retry=query_task_conversation(common,tasks,catalog,interaction,interaction_catalog,access,"player",0,preview);assert(retry.ok);const auto& first=field(retry.value,"rows").as<V::Array>()->front();assert(*field(first,"status_text").as<StableId>()=="受领中断，尚未应承");assert(!first.find("deadline_tick"));assert(*field(field(first,"actions").as<V::Array>()->front(),"command").as<StableId>()=="accept_offer");tasks.tasks.erase(interrupted.id);common.state().wallets.erase(interrupted.budget_wallet_id);
    // 已完成劳动的真实治理责任仍在本人Task账中，但不能遮掉当前真人可办的普通委托。
    GovernanceTaskState governance;TaskRecord governed;governed.id="task:18";governed.serial=18;governed.offer_id="authority";governed.assignee="player";governed.status="ready";governed.accepted_tick=1;governed.deadline_tick=13;governed.objective_completed_tick=7;tasks.tasks.insert(governed.id,governed);
    GovernanceTaskContract gov;auto& accepted=gov.acceptance;accepted.fact_id="gov.accepted";accepted.task_id=governed.id;accepted.contract_id="gov.contract";accepted.authority_source_fact_id="authority";accepted.organization_id="org";accepted.executor_actor_id="player";accepted.jurisdiction_id="county";accepted.site_id="office";accepted.rule_id="appease";accepted.rule_version="1";accepted.relief_terms_id="relief";accepted.reservation_id="gov.contract";accepted.display_name="安民";accepted.accepted_tick=1;accepted.accepted_revision=5;accepted.work_ticks=6;accepted.deadline_tick=13;gov.action_id="action:17";gov.work=GovernanceWorkCompletion{"gov.work",governed.id,"gov.contract","action:17","gov.accepted","gov.plan",1,7};governance.governance_contracts.insert(governed.id,gov);
    assert(common.append_fact({"gov.accepted","governance_task_accepted","tasks"}).ok);assert(common.append_fact({"authority","governance_authority_issued","politics"}).ok);assert(common.append_fact({"gov.work","governance_work_completed","tasks"}).ok);assert(common.append_fact({"gov.plan","governance_outcome_plan","politics"}).ok);assert(validate_governance_tasks(common,tasks,governance).ok);
    const auto common_before_governance=encode_common_state(common.state()),tasks_before_governance=encode_task_state(tasks);
    auto with_governance=query_task_conversation_with_governance(common,tasks,catalog,interaction,interaction_catalog,access,"player",0,preview,governance,true);assert(with_governance.ok&&field(with_governance.value,"rows").as<V::Array>()->size()==2);
    assert(query_task_conversation_with_governance(common,tasks,catalog,interaction,interaction_catalog,access,"player",0,preview,governance,false).error.original.code=="GovernanceTaskProfileRequired");
    GovernanceTaskState missing_governance;assert(query_task_conversation_with_governance(common,tasks,catalog,interaction,interaction_catalog,access,"player",0,preview,missing_governance,true).error.original.code=="UnknownOffer");
    auto forged_governance=governance;forged_governance.governance_contracts.find(governed.id)->acceptance.authority_source_fact_id="forged";assert(query_task_conversation_with_governance(common,tasks,catalog,interaction,interaction_catalog,access,"player",0,preview,forged_governance,true).error.original.code=="InvalidGovernanceTaskSource");
    auto unknown=letter;unknown.id="unknown";unknown.offer_id="missing.offer";tasks.tasks.insert(unknown.id,unknown);assert(query_task_conversation_with_governance(common,tasks,catalog,interaction,interaction_catalog,access,"player",0,preview,governance,true).error.original.code=="UnknownOffer");tasks.tasks.erase(unknown.id);
    assert(compare_boundary(common_before_governance,encode_common_state(common.state())).exact()&&compare_boundary(tasks_before_governance,encode_task_state(tasks)).exact());tasks.tasks.erase(governed.id);
    // 分页保全全部本人线索；绝不因为首屏16条就静默丢失其余委托。
    for(int index=0;index<20;++index){auto offer=*catalog.offers.find("grain");offer.id="extra"+std::to_string(index);catalog.offers.insert(offer.id,offer);interaction.knowledge.front().offers.push_back({offer.id,0,{"prop","board"}});}
    auto page=query_task_conversation(common,tasks,catalog,interaction,interaction_catalog,access,"player",0,preview);assert(page.ok&&field(page.value,"rows").as<V::Array>()->size()==16);auto next=query_task_conversation(common,tasks,catalog,interaction,interaction_catalog,access,"player",*field(page.value,"next_cursor").as<Integer>(),preview);assert(next.ok&&field(next.value,"rows").as<V::Array>()->size()==6&&!next.value.find("next_cursor"));
    std::cout<<"task_conversation: known-source isolation, multiple roles, real context guard, original quote errors, read purity and pagination passed\n";
}

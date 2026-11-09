// 共同底座定向门覆盖真实新人物、组织准入、玩家互斥、暂停恢复、旧責任及冷存；不代替完整游戏门。
#include "resident/domains/director/director_model.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
#include "resident/views/world_observation.hpp"
#include "resident/domains/social/social_model.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include "resident/domains/growth/growth_life_results.hpp"
#include "resident/domains/travel/travel.hpp"
#include "resident/domains/organization/organization_model.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <iostream>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

using namespace hanmo::resident;
namespace {
// 本门无战场或会话；实际行动占用仍读取CommonStore，不以替身返回值模拟业务结果。
struct NoActivities final:ActivityRead {
    bool in_open_battle(const StableId&)const override{return false;}
    bool in_interaction(const StableId&,const StableId&)const override{return false;}
};

// 新人物沿受控资源recipe及create_actor原语进入同一共同表。
void create_person(CommonStore& common,const StableId& id) {
    const Owner owner{Owner::Kind::actor,id,{}};
    ActorRecord actor;actor.id=id;actor.alive=true;actor.available=true;actor.hp=100;
    actor.wallet_id="wallet:"+id;actor.inventory_id="bag:"+id;actor.location=SettlementLocation{"town","market"};
    ResourceRecipe resources;resources.operations.push_back(CreateWallet{{actor.wallet_id,owner,100,{},{}}});resources.operations.push_back(CreateInventory{{actor.inventory_id,owner,100,0,{},{}}});
    assert(common.apply_resources(resources).ok);assert(common.create_actor(actor).ok);
}

// 全量域字节用于确认重复登记、失效与冷读不损失公平次序或历史。
CodecVisitor::Buffer registry_bytes(DirectorState state) {
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);
    assert(visit_director_checkpoint(writer,state,state.regional_life,state.named_membership,true).ok&&writer.finish().ok);return bytes;
}
// 该适配器只投影真实经济域已注册的事实，不制造测试专用付款或欠据。
struct EconomicResults final:DynamicResultRead {
    const CommonStore& common;const LocalEconomyState& economy;
    EconomicResults(const CommonStore& c,const LocalEconomyState& e):common(c),economy(e){}
    const DeliveryFact* delivery(const StableId&)const override{return nullptr;}
    std::optional<DynamicFactView> fact(const StableId& id)const override{return local_economy_dynamic_fact(economy,id);}
    std::vector<StableId> fact_ids(Integer after,Integer through)const override{std::vector<StableId> ids;for(const auto& row:common.state().facts.rows()){auto value=fact(row.first);if(value&&value->tick>after&&value->tick<=through)ids.push_back(row.first);}return ids;}
};

// 工资20实付7形成责任，后续实付13关闭同一欠据；下个周期另立责任且不重放旧成果。
void verify_obligation_need_source(){
    auto common_catalog=std::make_shared<CommonCatalog>();common_catalog->organization_ids={"city"};CommonState initial;initial.metadata.player_actor_id="player";CommonStore common(std::move(initial),common_catalog);
    create_person(common,"player");create_person(common,"worker");
    ResourceRecipe treasury;treasury.operations.push_back(CreateWallet{{"city.wallet",{Owner::Kind::organization,"city",{}},7,{},{}}});assert(common.apply_resources(treasury).ok);
    LocalEconomyCatalog catalog;catalog.enabled=true;catalog.named_actors_excluded={"player","worker"};catalog.rates.insert("wage",{"wage",20,1,false});
    RecurringObligation wage;wage.id="worker.salary";wage.principal={SubjectKind::organization,"city"};wage.beneficiary={SubjectKind::actor,"worker"};wage.source_wallet_or_inventory="city.wallet";wage.destination="wallet:worker";wage.rate_profile="wage";wage.authority_ref="institution";wage.period_ticks=2;wage.next_due_tick=1;catalog.obligations.insert(wage.id,wage);
    auto economy_initial=make_initial_local_economy(common,catalog,0);assert(economy_initial.ok);auto economy=std::move(economy_initial.value);
    OrganizationCatalog org_catalog;org_catalog.enabled=true;OrganizationRecord org;org.id="city";org.kind="city";org_catalog.organizations.insert(org.id,org);org_catalog.institutional_issuers.insert("institution",{"institution","city","continue_under_organization",{wage.id},{"city.wallet"},100});
    OrganizationState organizations;organizations.organizations=org_catalog.organizations;OrganizationAccess authority(common,organizations,org_catalog,economy);NoActivities activity;EconomicResults results(common,economy);TaskState tasks;
    AffairsCatalog policy;policy.obligation_needs=true;policy.obligation_policies.insert("salary-follow",{"salary-follow",wage.id,"city","salary"});auto affairs_initial=make_initial_affairs(policy);assert(affairs_initial.ok);auto affairs=std::move(affairs_initial.value);
    const auto step=[&](Integer tick){auto advanced=step_local_economy(common,economy,catalog,authority,activity,{tick-1,tick,{},{},{}});if(!advanced.ok)std::cerr<<advanced.error.original.code<<"\n";assert(advanced.ok);common.state().identity.tick=tick;auto updated=reconcile_affairs(common,affairs,policy,tasks,{},results,tick);if(!updated.ok)std::cerr<<updated.error.original.code<<"\n";assert(updated.ok);auto valid=validate_affairs(common,affairs,policy,tasks,&results);if(!valid.ok)std::cerr<<valid.error.original.code<<"\n";assert(valid.ok);};
    step(1);assert(affairs.needs.size()==1&&tasks.offer_instances.empty());const auto first=affairs.needs.rows().front().first;
    assert(affairs.needs.find(first)->target_amount==20&&affairs.needs.find(first)->fulfilled_amount==7&&need_gap(*affairs.needs.find(first))==13&&common.wallet("wallet:worker")->balance==107);
    // 真实已付7不能被冷档退写为尚未支付；删除整个责任也不能抹去原经济事实。
    auto omitted_payment=affairs;auto* rollback_need=omitted_payment.needs.find(first);rollback_need->fulfilled_amount=0;rollback_need->status=NeedStatus::open;rollback_need->source_fact_id=rollback_need->receipt_ids.front();rollback_need->receipt_ids.resize(1);rollback_need->source_revision=1;
    auto omitted_need=affairs;const auto affair_id=omitted_need.needs.find(first)->affair_id;omitted_need.needs.erase(first);omitted_need.affairs.erase(affair_id);
    const auto invalid_payment=validate_affairs(common,omitted_payment,policy,tasks,&results),invalid_missing=validate_affairs(common,omitted_need,policy,tasks,&results);
    std::cerr<<"omitted_payment_accepted="<<invalid_payment.ok<<" omitted_need_accepted="<<invalid_missing.ok<<"\n";assert(!invalid_payment.ok&&!invalid_missing.ok);
    const auto sequence=common.state().next_instance_id;assert(reconcile_affairs(common,affairs,policy,tasks,{},results,1).ok&&common.state().next_instance_id==sequence);
    KnownSubjectView known;known.actor_id="worker";known.subject={SubjectKind::actor,"worker"};auto forbidden=preview_need_claim(common,affairs,tasks,known,first,1);assert(!forbidden.ok&&forbidden.error.kind==ErrorKind::unsupported_capability&&common.state().next_instance_id==sequence);
    assert(common.apply_resources({{WalletDelta{"wallet:player",-13,true},WalletDelta{"city.wallet",13,true}},{}}).ok);step(2);
    assert(affairs.needs.find(first)->status==NeedStatus::fulfilled&&common.wallet("wallet:worker")->balance==120);
    step(3);assert(affairs.needs.size()==2&&affairs.needs.find(first)->fulfilled_amount==20&&affairs.needs.find(first)->status==NeedStatus::fulfilled);
    const auto& second=affairs.needs.rows().back().second;assert(second.arrear_id!=affairs.needs.find(first)->arrear_id&&need_gap(second)==20);
    economy.obligations.find(wage.id)->active=false;assert(reconcile_affairs(common,affairs,policy,tasks,{},results,3).ok&&need_gap(affairs.needs.rows().back().second)==20);
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);assert(visit_affairs_checkpoint(writer,affairs,true).ok&&writer.finish().ok);AffairsState restored;const auto& input=bytes;CodecVisitor reader(input);assert(visit_affairs_checkpoint(reader,restored,true).ok&&reader.finish().ok&&validate_affairs(common,restored,policy,tasks,&results).ok);
    auto decoded=decode_affairs_state(encode_affairs_state(affairs),true);assert(decoded.ok&&compare_boundary(encode_affairs_state(decoded.value),encode_affairs_state(affairs)).exact());assert(!decode_affairs_state(encode_affairs_state(affairs),false).ok);
    restored.needs.find(first)->fulfilled_amount=19;assert(!validate_affairs(common,restored,policy,tasks,&results).ok);
}
}

// 只读适配器直取原GrowthState已登记结果，测试可注入坏来源以验证收尾拒绝。
struct LifeSource final:GrowthLifeRead {
    const GrowthState& state;explicit LifeSource(const GrowthState& value):state(value){}
    std::optional<LifeActivityView> life_result(const StableId& id)const override{return growth_life_result(state,id);}
    std::vector<StableId> life_fact_ids(Integer after,Integer through)const override{return growth_life_fact_ids(state,after,through);}
};

// 已结束的生活计划保原政策归档；离职后新政策可继续合法候选，不重放旧成果。
void verify_policy_rebinding(){
    auto definitions=std::make_shared<CommonCatalog>();definitions->items.insert("grain",{"grain",1,1});definitions->roles.insert("work",{"work","a","market",{},{},{},0});CommonState seed;seed.metadata.player_actor_id="player";CommonStore common(std::move(seed),definitions);create_person(common,"player");create_person(common,"a");
    OrganizationState organizations;OrganizationRecord city;city.id="city";organizations.organizations.insert(city.id,city);OrgMemberState member;member.id="city:a";member.organization_id="city";member.actor_id="a";organizations.members.insert(member.id,member);common.state().roles.insert("work",{"a",true,{}});
    OrganizationCatalog org_catalog;LocalEconomyState economy;OrganizationAccess access(common,organizations,org_catalog,economy);NoActivities activity;TaskState tasks;
    DirectorCatalog catalog;catalog.dynamic_subjects=true;catalog.regional_life=true;DirectorPolicy p;p.organization_id="city";p.templates={DirectorTemplate::private_livelihood};p.private_livelihood=PrivateLivelihoodPolicy{"grain",1,0,80,{"work"},"market"};catalog.policies.insert("employed",p);DirectorPolicy fallback;fallback.templates={DirectorTemplate::maintain_commitment};catalog.policies.insert("independent",fallback);catalog.admissions.insert("membership",{"employed","city",DirectorAdmissionKind::organization_members,18,{}});common.state().actors.find("a")->birth_tick=-20*720;catalog.admissions.insert("fallback",{"independent",{},DirectorAdmissionKind::all_actors,{}});
    DirectorState state;state.dynamic_subjects=true;state.regional_life=true;assert(reconcile_director_registry(common,state,catalog,access,tasks,activity).ok);
    FinitePlan plan;plan.policy_id="employed";plan.id="plan:a:1";plan.subject={SubjectKind::actor,"a"};plan.template_id="private_livelihood";plan.phase=PlanPhase::completed;plan.livelihood=LivelihoodIntent{"supplies","grain",1,{},{},{},0,"market"};state.plans.insert("a",plan);assert(validate_director(common,state,catalog).ok);
    // 一个尚待消费的终态阻止政策切换；取消保原因，成功只结清单步，不升级整个目标。
    DirectorState pending=state;pending.plans.find("a")->pending_life_action_id="life:source";GrowthState growth;LifeActivityRecord record;record.view.fact_id="life:source";record.view.action_id="life:source";record.view.actor_id="a";record.view.phase=LifeActivityPhase::cancelled;record.view.completed_tick=0;record.view.code="CancelledByActor";growth.life_activities.insert("life:source",record);assert(common.append_fact({"life:source","life_activity","growth"}).ok);LifeSource life(growth);GrowthCatalog growth_catalog;TravelCatalog travel;TaskCatalog task_catalog;AffairsState affairs;SocialState social;
    organizations.members.find("city:a")->response=MemberResponse::departed;assert(reconcile_director_registry(common,pending,catalog,access,tasks,activity).ok&&pending.registrations.find("a")->obligations_only&&pending.registrations.find("a")->policy_id=="employed"&&pending.archived_plans.empty());
    const auto view=known_subject_view(common,social,access,"a");const auto propose=[&](){return propose_director(common,pending,catalog,view,access,affairs,tasks,task_catalog,*definitions,growth_catalog,travel,organizations,org_catalog,0,nullptr,&life,nullptr);};
    auto cancelled=propose();assert(cancelled.ok&&cancelled.value.size()==1&&!cancelled.value[0].command&&cancelled.value[0].transition->phase==PlanPhase::waiting&&cancelled.value[0].transition->wait_reason=="CancelledByActor"&&!cancelled.value[0].transition->pending_life_action_id);
    auto* terminal=growth.life_activities.find("life:source");terminal->view.phase=LifeActivityPhase::completed;terminal->view.success=true;terminal->view.settlement_tick=-1;auto unsettled=propose();assert(!unsettled.ok&&unsettled.error.original.code=="UnsettledLifeActionResult");terminal->view.settlement_tick=0;
    auto completed=propose();assert(completed.ok&&completed.value.size()==1&&!completed.value[0].command&&completed.value[0].transition->phase==PlanPhase::waiting&&completed.value[0].transition->wait_reason=="PolicyChangeAfterSettlement");
    assert(record_director_result(pending,completed.value[0],{DecisionOutcome::Kind::adopted,{}, {},{}},0).ok);assert(reconcile_director_registry(common,pending,catalog,access,tasks,activity).ok&&pending.archived_plans.find("plan:a:1")&&!pending.archived_plans.find("plan:a:1")->pending_life_action_id&&pending.registrations.find("a")->policy_id=="independent");
    assert(reconcile_director_registry(common,state,catalog,access,tasks,activity).ok);
    const auto valid=validate_director(common,state,catalog);if(!valid.ok)std::cerr<<valid.error.original.code<<"\n";assert(valid.ok);
    assert(!state.plans.find("a")&&state.archived_plans.find("plan:a:1")&&state.archived_plans.find("plan:a:1")->policy_id=="employed"&&state.registrations.find("a")->policy_id=="independent");
    const auto saved=registry_bytes(state);auto decoded=decode_director_seed(encode_director_state(state),true,false,true);assert(decoded.ok&&registry_bytes(decoded.value)==saved&&validate_director(common,decoded.value,catalog).ok);
    DirectorState restored;CodecVisitor reader(saved);assert(visit_director_checkpoint(reader,restored,true,false,true).ok&&reader.finish().ok&&restored.archived_plans.size()==1&&registry_bytes(restored)==saved&&validate_director(common,restored,catalog).ok);
}

// 分页预算包括不可见项；旧身份、远地与他人观察请求均不能续用当前玩家页面。
void verify_world_observation(){
    auto definitions=std::make_shared<CommonCatalog>();CommonState initial;initial.metadata.player_actor_id="player";initial.identity.world_id[0]=7;initial.identity.execution_epoch=3;CommonStore common(std::move(initial),definitions);create_person(common,"player");create_person(common,"near");
    for(Integer n=0;n<300;++n){const auto id="remote:"+std::to_string(n);create_person(common,id);common.state().actors.find(id)->location=SettlementLocation{"other","hidden"};}
    SocialState social;assert(initialize_dynamic_social(social).ok);TaskState tasks;InteractionState interaction;
    Report report;report.id="report:supply";report.fact_id="supply";report.payload_kind="local_supply_observation";report.origin_subject={SubjectKind::organization,"city"};report.visibility_scope=ReportVisibility::local_public;report.observation.id="supply";report.observation.kind=report.payload_kind;report.observation.publicly_visible=true;report.observation.supply=SupplyObservation{"source","city","market","public.stock","grain","supply",20,7,1};social.dynamic->reports.insert(report.id,report);
    Report secret=report;secret.id="report:private";secret.fact_id="private";secret.visibility_scope=ReportVisibility::participants;secret.observation.publicly_visible=false;secret.observation.supply->source_key="private";social.dynamic->reports.insert(secret.id,secret);
    CodecVisitor::Buffer common_before;CodecVisitor before_writer(common_before);auto common_copy=common.state();assert(visit_common_checkpoint(before_writer,common_copy).ok&&before_writer.finish().ok);const auto social_before=encode_social_state(social);
    // 无关远地数量不改变合法序列；另外加入可见本人责任使页界真实可测。
    for(Integer n=0;n<70;++n){TaskRecord task;task.id="own:"+std::to_string(n);task.assignee="player";task.status="active";tasks.tasks.insert(task.id,task);}
    const auto index=build_world_observation_index(common,social,tasks);
    auto first=query_world_observation(common,social,tasks,interaction,index,"player","market");assert(first.ok&&first.value.rows.size()==world_observation_page_limit&&first.value.scanned<=world_observation_scan_limit&&first.value.next_cursor);
    const auto rendered=render_boundary_json(encode_world_observation(first.value));assert(rendered.ok&&rendered.value.find("remote:")==std::string::npos&&rendered.value.find("private")==std::string::npos);
    auto page=query_world_observation(common,social,tasks,interaction,index,"player","market",first.value.next_cursor);assert(page.ok&&!page.value.next_cursor&&page.value.rows.size()==9&&std::get<ObservedSupplySummary>(first.value.rows[2]).source.quantity==7);
    auto encoded=encode_world_observation(first.value);auto decoded=decode_world_observation_cursor(*encoded.find("next_cursor"));assert(decoded.ok&&decoded.value.offset==first.value.next_cursor->offset);
    assert(!query_world_observation(common,social,tasks,interaction,index,"player","hidden").ok&&!query_world_observation(common,social,tasks,interaction,index,"near","market").ok);
    auto stale=*first.value.next_cursor;--stale.identity.execution_epoch;assert(!query_world_observation(common,social,tasks,interaction,index,"player","market",stale).ok);stale=*first.value.next_cursor;++stale.identity.revision;assert(!query_world_observation(common,social,tasks,interaction,index,"player","market",stale).ok);
    CodecVisitor::Buffer common_after;CodecVisitor after_writer(common_after);common_copy=common.state();assert(visit_common_checkpoint(after_writer,common_copy).ok&&after_writer.finish().ok&&common_after==common_before&&compare_boundary(social_before,encode_social_state(social)).exact());
}

int main() {
    verify_obligation_need_source();
    verify_policy_rebinding();
    verify_world_observation();
    auto common_catalog=std::make_shared<CommonCatalog>();CommonState seed;seed.metadata.player_actor_id="player";
    CommonStore common(std::move(seed),common_catalog);create_person(common,"player");create_person(common,"a");create_person(common,"b");
    OrganizationState organizations;OrganizationCatalog organization_catalog;LocalEconomyState economy;
    OrganizationRecord city;city.id="city";organizations.organizations.insert(city.id,city);
    OrgMemberState member;member.id="city:a";member.organization_id="city";member.actor_id="a";organizations.members.insert(member.id,member);
    OrganizationAccess access(common,organizations,organization_catalog,economy);TaskState tasks;NoActivities activities;
    DirectorCatalog catalog;catalog.dynamic_subjects=true;catalog.decision_budget=1;
    DirectorPolicy policy;policy.organization_id="city";policy.templates={DirectorTemplate::maintain_commitment,DirectorTemplate::await_adjustment};catalog.policies.insert("city-policy",policy);
    DirectorAdmission admission;admission.policy_id="city-policy";admission.organization_id="city";admission.kind=DirectorAdmissionKind::organization_members;catalog.admissions.insert("city-members",admission);
    DirectorState state;state.dynamic_subjects=true;
    assert(reconcile_director_registry(common,state,catalog,access,tasks,activities).ok);
    assert(state.registrations.size()==1&&director_subject_active(state,"a")&&!director_subject_active(state,"b"));
    const auto once=registry_bytes(state);assert(reconcile_director_registry(common,state,catalog,access,tasks,activities).ok&&registry_bytes(state)==once);
    // 名单创建后自动登记；目录不增加逐actor策略或重复人物。
    create_person(common,"new");member.id="city:new";member.actor_id="new";organizations.members.insert(member.id,member);
    assert(reconcile_director_registry(common,state,catalog,access,tasks,activities).ok&&director_subject_active(state,"new"));
    const auto entered=state.registrations.find("a")->epoch;
    assert(common.set_player_actor("a").ok);assert(reconcile_director_registry(common,state,catalog,access,tasks,activities).ok);
    assert(!director_subject_active(state,"a")&&state.registrations.find("a")->reason=="PlayerControlled"&&state.registrations.find("a")->epoch>entered);
    assert(common.set_player_actor("player").ok);assert(reconcile_director_registry(common,state,catalog,access,tasks,activities).ok&&director_subject_active(state,"a"));
    common.state().actors.find("a")->action_id="existing";assert(reconcile_director_registry(common,state,catalog,access,tasks,activities).ok&&!director_subject_active(state,"a"));
    common.state().actors.find("a")->action_id.clear();assert(reconcile_director_registry(common,state,catalog,access,tasks,activities).ok&&director_subject_active(state,"a"));
    TaskRecord task;task.id="frozen";task.assignee="a";task.status="active";tasks.tasks.insert(task.id,task);
    organizations.members.find("city:a")->response=MemberResponse::departed;
    assert(reconcile_director_registry(common,state,catalog,access,tasks,activities).ok&&director_subject_active(state,"a")&&state.registrations.find("a")->obligations_only);
    tasks.tasks.find("frozen")->status="completed";common.state().actors.find("a")->action_id="existing-duty";assert(reconcile_director_registry(common,state,catalog,access,tasks,activities).ok&&!director_subject_active(state,"a"));assert(state.registrations.find("a")->lifecycle==DirectorLifecycle::retired&&state.registrations.find("a")->reason=="AdmissionWithdrawn");
    assert(common.wallet("wallet:a")->balance==100&&tasks.tasks.find("frozen")&&state.schedules.find("a"));
    common.state().actors.find("new")->alive=false;assert(reconcile_director_registry(common,state,catalog,access,tasks,activities).ok&&state.registrations.find("new")->reason=="ActorDead");
    catalog.admissions.find("city-members")->minimum_age_years=18;common.state().actors.find("new")->alive=true;common.state().actors.find("new")->birth_tick=0;
    assert(reconcile_director_registry(common,state,catalog,access,tasks,activities).ok&&state.registrations.find("new")->reason=="ActorBelowPolicyAge"&&!director_subject_active(state,"new"));
    common.state().actors.find("new")->birth_tick.reset();assert(reconcile_director_registry(common,state,catalog,access,tasks,activities).ok&&state.registrations.find("new")->reason=="ActorAgeUnknown");
    common.state().actors.find("new")->birth_tick=-20*720;common.state().actors.find("new")->available=false;assert(reconcile_director_registry(common,state,catalog,access,tasks,activities).ok&&state.registrations.find("new")->reason=="ActorUnavailable");
    const auto saved=registry_bytes(state);DirectorState restored;CodecVisitor reader(saved);
    assert(visit_director_checkpoint(reader,restored,false,false,true).ok&&reader.finish().ok&&registry_bytes(restored)==saved);
    assert(validate_director_registry(common,restored,catalog).ok);
    auto decoded=decode_director_seed(encode_director_state(state),false,false,true);assert(decoded.ok&&registry_bytes(decoded.value)==saved);
    catalog.admissions.find("city-members")->kind=static_cast<DirectorAdmissionKind>(99);
    const auto rejected=reconcile_director_registry(common,state,catalog,access,tasks,activities);
    assert(!rejected.ok&&rejected.error.kind==ErrorKind::unsupported_capability&&registry_bytes(state)==saved);
    std::cout<<"shared_world_foundation: PASS (dynamic identity, policy archive, age and source gates, bounded private observation, real wage needs, ownership exclusion, pause/resume, frozen responsibility, nonmutation, cold state)\n";
}

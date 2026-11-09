// 原Office/Grant与真实政治签发构成最小来源；本门不冒完整Task赈济或World运行。
#include "resident/domains/politics/governance_intents/political_governance_director.hpp"
#include "resident/kernel/governance_integration.hpp"
#include "resident/domains/politics/governance_intents/political_governance_intent_validation.hpp"
#include "resident/kernel/command_types.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
Integer checks=0;
// 每项失败保留具名断言，不以编译通过替代行为运行。
void check(bool condition,const char* text){++checks;if(!condition)throw std::runtime_error(text);}
// 测试沿原正式签发拒因，不手工生成成功邀请。
template<class T>T take(Outcome<T> v){if(!v.ok)throw std::runtime_error(v.error.original.code);return std::move(v.value);}
// 无交战/会话的局部环境；真实共同动作资格由原入口检查。
struct Activity final:ActivityRead {bool in_open_battle(const StableId&)const override{return false;}bool in_interaction(const StableId&,const StableId& = {})const override{return false;}};
// 最小原政治权源，不含任何预置Task或政治邀请事实。
struct Fixture {
 std::shared_ptr<CommonCatalog> cc=std::make_shared<CommonCatalog>();CommonStore common{CommonState{},cc};PoliticsCatalog pc;PoliticsState politics;PoliticsGovernanceCatalog gc;PoliticsGovernanceState governance;OrganizationCatalog oc;OrganizationState org;LocalEconomyState economy;PublicReliefCatalog relief_catalog;PublicReliefState relief;TaskState tasks;GovernanceTaskState governance_tasks;Activity activity;TravelCatalog travel;
 PoliticalGovernanceIntent intent{"once","issuer","worker","office","county","policy","granary"};DirectorPolicy policy;
 Fixture(){
  cc->actor_ids={"issuer","worker","other"};cc->organization_ids={"city"};cc->items.insert("grain",{"grain",1,1});cc->texts.insert("appease.name","安民");cc->places.insert("hall",{"hall","town",{}});common.state().identity.world_id[0]=9;
  for(const auto& id:cc->actor_ids){ActorRecord a;a.id=id;a.hp=100;a.alive=a.available=true;a.birth_tick=-14400;a.location=SettlementLocation{"town","hall"};common.state().actors.insert(id,a);}
  OrganizationRecord city;city.id="city";city.kind="political";city.headquarters_site="hall";oc.enabled=true;oc.organizations.insert(city.id,city);org.organizations=oc.organizations;
  OfficeState office;office.id="office";office.organization_id="city";office.holder_actor_id="issuer";office.grant_ids={"grant"};oc.offices.insert(office.id,office);org.offices=oc.offices;
  Grant g;g.id="grant";g.office_id="office";g.amount_limit=120;g.may_delegate=true;g.allowed_command_kinds={"political_commission_governance"};g.scopes={{ScopeRef::Kind::site,"hall"}};g.wallet_ids={"public"};g.inventory_ids={"granary"};g.continuity_policy_id="stop_when_vacant";oc.grants.insert(g.id,g);
  MemberPolicy mp;mp.id="member.policy";oc.member_policies.insert(mp.id,mp);for(const auto& id:{"issuer","worker"}){OrgMemberState m;m.id=id;m.actor_id=id;m.organization_id="city";m.policy_id=mp.id;oc.members.insert(m.id,m);}org.members=oc.members;
  common.state().wallets.insert("public",{"public",{Owner::Kind::organization,"city",{}},200,{}});common.state().inventories.insert("granary",{"granary",{Owner::Kind::organization,"city",{}},100,0,{{"grain.stock","grain",{},10,{Owner::Kind::organization,"city",{}}}}});
  Jurisdiction j;j.id="county";j.office_id="office";j.seat_place_id="hall";j.public_wallet_id="public";j.public_inventory_ids={"granary"};j.political_organization_id=j.controlling_organization_id="city";pc.enabled=true;pc.jurisdictions.insert(j.id,j);politics.jurisdictions=pc.jurisdictions;
  gc.enabled=true;gc.appeasement_rules.insert("appease",{"appease","1","appease.name","grain",3,120,10,3,100});gc.commission_policies.insert("policy",{"policy","appease","relief",4,20});policy.organization_id="city";
 }
 // 真实家庭与唯一原份额计划使原三域preview可达；不提供替代政治许可。
 void enable_relief(){common.state().wallets.insert("family.cash",{"family.cash",{Owner::Kind::household,"family",{}},0,{}});common.state().inventories.insert("family.stock",{"family.stock",{Owner::Kind::household,"family",{}},100,0,{}});economy.households.insert("family",{"family","family.cash","family.stock"});PopulationCohort c;c.id="cohort";c.site_id="hall";c.household_group_id="family";c.headcount=c.labor_capacity=1;c.private_wallet_id="family.cash";c.private_inventory_id="family.stock";economy.cohorts.insert(c.id,c);relief_catalog.policies.insert("relief",{"relief","hall","city","grain",{{"family","cohort",120,10}}});}
 // 同步借用，不把临时Context存入返回提案。
 GovernanceReadContext read()const{return {common,*cc,politics,pc,governance,gc,org,oc,economy,relief_catalog,relief,tasks,governance_tasks,activity};}
 // 只构造当前本人已知身份；无远方真人观察。
 KnownSubjectView known(const StableId& id)const{KnownSubjectView k;k.actor_id=id;k.subject={SubjectKind::actor,id};k.tick=common.tick();k.revision=common.revision();k.alive=k.available=true;k.place_id="hall";return k;}
 // 原预算的排期字段用醒目标值证明提案没有重设第二轮调度。
 DecisionProposal base(const StableId& id)const{DecisionProposal p;p.subject_id=id;p.plan_id="plan."+id;auto t=std::make_shared<DirectorTransition>();t->expected_revision=common.revision();t->next_review_tick=17;t->decision_seq=1;p.transition=t;return p;}
 // 原业务实际写入Common头与不可变批准，不经过新意图writer。
 GovernanceAuthorityApproval issue(){return take(issue_governance_authority(common,*cc,politics,governance,gc,org,oc,activity,"issuer",{{"city","office",0,"issuer"},"worker","county","policy","granary",0}));}
};
}
int main(){try{
 Fixture f;PoliticalGovernanceIntentCatalog catalog;catalog.intents.insert(f.intent.id,f.intent);check(validate_political_governance_intents(catalog,*f.cc,f.pc,f.gc,f.oc).ok,"exact original policy and complete grant accepted");auto encoded=encode_political_governance_intents(catalog);check(take(decode_political_governance_intents(encoded)).intents.size()==1,"explicit v1 catalog roundtrip");auto duplicate=f.intent;duplicate.id="second";catalog.intents.insert(duplicate.id,duplicate);check(!validate_political_governance_intents(catalog,*f.cc,f.pc,f.gc,f.oc).ok,"same pair cannot gain a second automatic issue");catalog.intents.erase("second");
 auto context=f.read();auto known=f.known("worker");auto base=f.base("worker");auto waiting=take(propose_director_governance_commission(context,f.travel,known,f.policy,f.intent,base,8));check(waiting&&!waiting->command&&waiting->transition->wait_reason=="GovernanceIntentAwaitingLocalCommission","receiver cannot invent original invitation");check(waiting->transition->next_review_tick==17,"original fair review budget preserved");
 known.interaction_busy=true;check(!take(propose_director_governance_commission(context,f.travel,known,f.policy,f.intent,base,8)),"other conversation retained");known.interaction_busy=false;known.commitments.push_back(KnownCommitment{"old.task","task","active","old.acceptance",20});check(!take(propose_director_governance_commission(context,f.travel,known,f.policy,f.intent,base,8)),"accepted responsibility precedes new work");known.commitments.clear();
 auto issued=f.issue();auto issuer=f.known("issuer");auto issuer_base=f.base("issuer");auto finished=take(propose_director_governance_commission(context,f.travel,issuer,f.policy,f.intent,issuer_base,8));check(finished&&!finished->command&&finished->transition->phase==PlanPhase::completed&&finished->transition->goal_ref==issued.authority_fact_id,"actual old issue fulfills one-time issuer intent without rewriting");check(validate_governance_intent_completed_source(context,f.intent,"issuer",issued.authority_fact_id,f.common.tick()).ok,"completed issuer binds actual source");check(!validate_governance_intent_completed_source(context,f.intent,"worker",issued.authority_fact_id,f.common.tick()).ok,"issued fact is not receiver acceptance");
 auto missing_assets=take(propose_director_governance_commission(context,f.travel,known,f.policy,f.intent,base,8));check(missing_assets&&!missing_assets->command&&!missing_assets->transition->wait_reason.empty(),"original public relief preview still blocks missing real plan");
 f.common.state().identity.tick=issued.offer_expires_tick;known=f.known("worker");base=f.base("worker");auto expired=take(propose_director_governance_commission(context,f.travel,known,f.policy,f.intent,base,8));check(expired&&!expired->command&&expired->transition->wait_reason=="GovernanceIntentOriginalOfferExpired","expired first invitation does not authorize new automatic issue");
 Fixture funded;funded.enable_relief();auto live=funded.read();auto signing=take(propose_director_governance_commission(live,funded.travel,funded.known("issuer"),funded.policy,funded.intent,funded.base("issuer"),8));check(signing&&signing->command&&signing->command->kind=="political_commission_governance"&&signing->transition->wait_reason.empty(),"real issuer command carries no incompatible waiting reason");
 auto actual=funded.issue();DirectorState director;ReviewSchedule schedule;schedule.subject={SubjectKind::actor,"issuer"};director.schedules.insert("issuer",schedule);DecisionOutcome outcome;outcome.kind=DecisionOutcome::Kind::adopted;check(record_director_result(director,*signing,outcome,funded.common.tick()).ok,"original Director accepts the actual issued-command transition");auto taking=take(propose_director_governance_commission(live,funded.travel,funded.known("worker"),funded.policy,funded.intent,funded.base("worker"),8));check(taking&&taking->command&&taking->command->kind=="governance_accept"&&taking->transition->wait_reason.empty(),"real funded acceptance preview returns original command without contradictory wait reason");
 // 归档保原复评排期；后来真实邀请不得填入此前已结束计划。
 Fixture history;history.common.state().identity.tick=3;auto later=history.issue();history.common.state().identity.tick=4;auto hc=history.read();DirectorCatalog dc;dc.dynamic_subjects=true;auto ic=std::make_shared<PoliticalGovernanceIntentCatalog>();ic->intents.insert(history.intent.id,history.intent);dc.governance_intents=ic;auto ip=history.policy;ip.templates={DirectorTemplate::governance_commission};ip.governance_intent_id="once";dc.policies.insert("issuer.policy",ip);DirectorState archived;FinitePlan fp;fp.id="archived";fp.subject={SubjectKind::actor,"issuer"};fp.policy_id="issuer.policy";fp.template_id=political_governance_intent_template;fp.goal_ref=later.authority_fact_id;fp.phase=PlanPhase::completed;fp.next_review_tick=3;archived.archived_plans.insert(fp.id,fp);check(!validate_political_governance_intent_director(dc,archived,&hc).ok,"later real invitation cannot backfill archived earlier review");archived.archived_plans.find(fp.id)->next_review_tick=4;check(validate_political_governance_intent_director(dc,archived,&hc).ok,"archived plan may retain actual invitation known at original review");auto current=archived;current.plans.insert("issuer",fp);current.archived_plans={};ReviewSchedule hs;hs.subject=fp.subject;hs.last_decision_tick=3;hs.next_tick=4;current.schedules.insert("issuer",hs);check(!validate_political_governance_intent_director(dc,current,&hc).ok,"current plan review must agree with actual schedule");
 std::cout<<checks<<" checks; no World or full public-relief path claimed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

// 生前真实业务writer门；首府控制及玩家历史为明确typed夹具，不冒World历史验证。
#define main succession_office_fixture_main
#include "political_succession_sources_test.cpp"
#undef main
#include "resident/domains/politics/succession/political_succession_nominations.hpp"
#include <map>
namespace {
// 不提供额外行动、会议或战场资格，原Common实际占用仍由原政治入口检查。
struct SuccessionActivity final:ActivityRead{bool in_open_battle(const StableId&)const override{return false;}bool in_interaction(const StableId&,const StableId& = {})const override{return false;}};
struct NominationFixture:Fixture {
 PoliticalSuccessionCatalog policies;PoliticalSuccessionNominationState nominations;LocalEconomyState economy;SuccessionActivity activity;std::map<StableId,StableId> historical_players;
 NominationFixture(){policies.policies.insert(policy.id,policy);MemberPolicy p;p.id="members";catalog.member_policies.insert(p.id,p);for(auto& row:common.state().actors.rows()){row.second.birth_tick=-14400;row.second.hp=100;row.second.location=SettlementLocation{"town","hall"};OrgMemberState member;member.id=row.first;member.actor_id=row.first;member.organization_id="city";member.policy_id=p.id;catalog.members.insert(member.id,member);}state.members=catalog.members;common.state().metadata.player_actor_id="leader";}
 // typed回调只隔离本域意愿规则；将来World须绑定真实控制/接管账，空读明确拒。
 PoliticalSuccessionNominationContext context(){return {common,*cc,state,catalog,economy,activity,policies,nominations,{},[](const PoliticalSuccessionPolicy&,Integer,const StableId&){return common_success();},[this](Integer,const StableId& fact)->Outcome<StableId>{auto i=historical_players.find(fact);return i==historical_players.end()?Outcome<StableId>{false,{},common_failure("FixturePlayerSourceMissing").error}:Outcome<StableId>{true,i->second,{}};}};}
 // 引用实际预览与同候选writer，测试历史控制只在真实新事实写出之后提供独立原本人。
 StableId submit(const StableId& actor,PoliticalSuccessionNominationCommand command){common.state().metadata.player_actor_id=actor;auto x=context();auto quote=take(preview_political_succession_nomination(x,actor,command));auto id=take(apply_political_succession_nomination_in_candidate(common,nominations,x,actor,command,quote));historical_players[id]=actor;check(validate_political_succession_nominations(context()).ok,"actual business source cold validation");return id;}
 PoliticalNominateSuccessor proposal(){return {{"city","chief",0,"leader"},"succession","candidate",0};}
};
}
#ifndef POLITICAL_SUCCESSION_NOMINATION_FIXTURE_ONLY
int main(){try{
 NominationFixture f;auto nomination=f.submit("leader",f.proposal());check(f.state.offices.find("chief")->holder_actor_id==std::optional<StableId>{"leader"}&&f.state.offices.find("chief")->epoch==0,"nomination does not confer public office");
 auto x=f.context();check(!preview_political_succession_nomination(x,"leader",f.proposal()).ok,"second pending nomination cannot overlap");
 f.common.state().metadata.player_actor_id="candidate";auto missing=f.context();missing.player_before={};check(!preview_political_succession_nomination(missing,"candidate",PoliticalAnswerSuccession{nomination,true}).ok,"new profile has no callback-free player proof");
 auto accepted=f.submit("candidate",PoliticalAnswerSuccession{nomination,true});check(f.nominations.answers.find(accepted)->accepted&&f.state.offices.find("chief")->epoch==0,"actual acceptance is only future willingness");
 auto declined=f.submit("candidate",PoliticalAnswerSuccession{nomination,false});check(f.nominations.answers.find(declined)->previous_acceptance_fact_id==accepted,"candidate may revoke only its exact earlier willingness");
 f.common.state().metadata.player_actor_id="candidate";check(!preview_political_succession_nomination(f.context(),"candidate",PoliticalAnswerSuccession{nomination,true}).ok,"closed proposal cannot be revived");
 auto next=f.submit("leader",f.proposal());auto withdrawn=f.submit("leader",PoliticalWithdrawSuccession{{"city","chief",0,"leader"},next});check(f.nominations.withdrawals.find(withdrawn)->nomination_fact_id==next,"original leader may explicitly withdraw new proposal");
 f.nominations.answers.find(declined)->previous_acceptance_fact_id.clear();check(!validate_political_succession_nominations(f.context()).ok,"renunciation cannot drop prior real acceptance");f.nominations.answers.find(declined)->previous_acceptance_fact_id=accepted;
 f.historical_players[accepted]="leader";check(!validate_political_succession_nominations(f.context()).ok,"receipt actor cannot replace historical controller");
 NominationFixture deputy;deputy.change("deputy");deputy.common.state().metadata.player_actor_id="deputy";auto p=deputy.proposal();p.authority.actor_id="deputy";p.authority.authority_epoch=p.expected_office_epoch=1;check(!preview_political_succession_nomination(deputy.context(),"deputy",p).ok,"temporary deputy cannot nominate formal dynasty successor");
 NominationFixture expiry;auto exp=expiry.submit("leader",expiry.proposal());expiry.common.state().identity.tick=4;expiry.common.state().metadata.player_actor_id="candidate";check(!preview_political_succession_nomination(expiry.context(),"candidate",PoliticalAnswerSuccession{exp,true}).ok,"acceptance must occur before original deadline");
 NominationFixture young;young.common.state().actors.find("candidate")->birth_tick=-720;check(!preview_political_succession_nomination(young.context(),"leader",young.proposal()).ok,"future independent office requires real adult qualification");
 std::cout<<checks<<" checks; typed historical callbacks, no World or office accession run claimed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#endif

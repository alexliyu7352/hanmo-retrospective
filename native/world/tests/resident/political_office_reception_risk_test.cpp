// 真实Common/Role与产品规则窄门；历史玩家/辖区为原ReceptionRulesFixture的明确隔离读。
#define POLITICAL_OFFICE_RECEPTION_RULES_FIXTURE_ONLY
#include "political_office_reception_rules_test.cpp"
namespace {
struct BusyCounter final:ActivityRead {
 bool in_open_battle(const StableId&)const override{return false;}
 bool in_interaction(const StableId& actor,const StableId& = {})const override{return actor=="leader";}
};
}
int main(){try{
 ReceptionRulesFixture f;f.install();f.common.state().metadata.player_actor_id="candidate";
 auto x=f.context();auto c=f.request();
 check(preview_political_office_reception(x,"candidate",c).ok,"actual installed incumbent can claim original public Role");
 auto bad=c;bad.authority.authority_epoch=0;check(!preview_political_office_reception(x,"candidate",bad).ok,"wrong epoch rejects before Role write");
 bad=c;bad.authority.actor_id="leader";check(!preview_political_office_reception(x,"candidate",bad).ok,"another actor's authority cannot be borrowed");
 auto allowed=f.catalog.grants.find("reception.grant")->allowed_command_kinds;f.catalog.grants.find("reception.grant")->allowed_command_kinds.clear();check(!preview_political_office_reception(f.context(),"candidate",c).ok,"no explicit reception Grant rejects");f.catalog.grants.find("reception.grant")->allowed_command_kinds=allowed;
 BusyCounter busy;auto bx=PoliticalOfficeReceptionContext{f.common,*f.cc,f.state,f.catalog,f.economy,busy,f.policies,f.receptions,f.reception_state,{},x.seat,x.player_before};check(!preview_political_office_reception(bx,"candidate",c).ok,"old provider's busy counter cannot be transferred");
 auto before=f.common.state();auto old=f.reception_state;auto token=take(f.common.begin_savepoint());auto id=f.assign();check(f.common.state().roles.find("public.counter")->actor_id=="candidate","actual Common Role changes before late failure");
 f.common.state().next_instance_id=INT64_MAX;auto failed=f.common.next_id("result");check(!failed.ok&&failed.error.original.code=="InstanceIdOverflow","actual result allocation fails after Role and fact write");take(f.common.rollback_savepoint(token));f.reception_state=old;f.historical_players.erase(id);check(f.common.state().next_instance_id==before.next_instance_id&&f.common.state().facts.order()==before.facts.order()&&f.common.state().roles.find("public.counter")->actor_id=="leader"&&f.reception_state.assignments.empty(),"outer candidate rollback restores Role ID and permanent source");
 id=f.assign();check(f.state.offices.find("chief")->epoch==1,"successful Role claim does not change Office epoch");auto head=*f.common.state().facts.find(id);f.common.state().facts.find(id)->domain="politics";check(!validate_political_office_reception_sources(f.context()).ok,"forged original-domain header rejects");*f.common.state().facts.find(id)=head;f.common.state().facts.find(id)->kind="organization_office_changed";check(!validate_political_office_reception_sources(f.context()).ok,"forged old Office header rejects");*f.common.state().facts.find(id)=head;check(validate_political_office_reception_sources(f.context()).ok,"restored real Role source remains cold-valid");
 std::cout<<checks<<" necessary reception assertions; isolated Common and domain source, no World\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

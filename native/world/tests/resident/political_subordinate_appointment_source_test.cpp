// 仅目录和原Office事实前界的源门；尚不代表完整补任writer、能力历史或World已运行。
#define POLITICAL_OFFICE_RESIGNATION_SOURCE_FIXTURE_ONLY
#include "political_office_resignation_source_test.cpp"
#include "resident/domains/politics/subordinate_appointments/political_subordinate_appointment_contract.hpp"
namespace {
struct SubordinateSourceFixture:ResignationSourceFixture {
 PoliticalSubordinateAppointmentCatalog appointments;
 SubordinateSourceFixture(){
  Grant g;g.id="appoint.county";g.office_id="chief";g.allowed_command_kinds={political_subordinate_offer_command,political_subordinate_withdraw_command};g.scopes={{ScopeRef::Kind::site,"hall"}};catalog.grants.insert(g.id,g);catalog.offices.find("chief")->grant_ids.push_back(g.id);state.offices.find("chief")->grant_ids.push_back(g.id);
  politics.appointment_policies.insert("competence",{"competence","qualified",5,8,1,0});appointments.policies.insert("ordinary",{"ordinary","county","succession",g.id,"competence"});
 }
 StableId offered(){auto id=take(common.next_id("political_subordinate_offer"));take(common.append_fact({id,"political_subordinate_appointment_offered",political_subordinate_appointment_module}));return id;}
 Status validate(){return validate_political_subordinate_appointment_catalog(common,*cc,politics,catalog,policies,appointments);}
};
}
#ifndef POLITICAL_SUBORDINATE_SOURCE_FIXTURE_ONLY
int main(){try{
 SubordinateSourceFixture f;check(f.validate().ok,"one explicit head-to-subordinate Office permission accepted");auto encoded=encode_political_subordinate_appointment_catalog(f.appointments);check(compare_boundary(encoded,encode_political_subordinate_appointment_catalog(take(decode_political_subordinate_appointment_catalog(encoded)))).exact(),"new catalog exact roundtrip");
 auto* grant=f.catalog.grants.find("appoint.county");grant->may_delegate=true;check(!f.validate().ok,"permission cannot become general subdelegation");grant->may_delegate=false;grant->wallet_ids={"public"};check(!f.validate().ok,"office-fill permission cannot acquire resource signing");grant->wallet_ids.clear();grant->scopes.front().id="other";check(!f.validate().ok,"head original capital is an exact permitted scope");grant->scopes.front().id="hall";
 f.appointments.policies.find("ordinary")->jurisdiction_id=f.policies.policies.find("succession")->jurisdiction_id;check(!f.validate().ok,"head Office has its own original succession rules");f.appointments.policies.find("ordinary")->jurisdiction_id="county";
 f.politics.jurisdictions.find("county")->revision=1;check(!f.validate().ok,"captured jurisdiction needs another explicit continuity rule");f.politics.jurisdictions.find("county")->revision=0;f.catalog.offices.find("county.office")->deputy_actor_id="candidate";check(!f.validate().ok,"deputy disposition is not assumed empty");f.catalog.offices.find("county.office")->deputy_actor_id.reset();
 auto first=f.offered();auto change=f.office_change();auto before=take(read_political_subordinate_office_before(f.common,f.state,f.catalog,"county.office",0,first));check(before.holder_actor_id==std::optional<StableId>{"candidate"}&&before.office_epoch==0,"later vacancy cannot backfill an earlier offer");auto later=f.offered();auto vacant=take(read_political_subordinate_office_before(f.common,f.state,f.catalog,"county.office",0,later));check(!vacant.holder_actor_id&&vacant.office_epoch==1&&vacant.last_change_fact_id==change,"later boundary consumes actual vacancy");
 auto order=f.common.state().facts;auto head=*f.common.state().facts.find(first);f.common.state().facts.erase(first);f.common.state().facts.insert(first,head);check(!read_political_subordinate_office_before(f.common,f.state,f.catalog,"county.office",0,first).ok,"same-tick Common reorder rejects reversed allocation history");f.common.state().facts=order;check(!read_political_subordinate_office_before(f.common,f.state,f.catalog,"county.office",0,change).ok,"arbitrary Office header cannot serve as appointment boundary");
 std::cout<<checks<<" subordinate appointment source assertions; not World\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#endif

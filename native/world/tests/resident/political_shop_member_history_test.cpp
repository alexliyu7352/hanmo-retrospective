// 成员纯历史来源夹具；不称为真实欠薪拒工命令或World运行。
#define main shop_member_legal_fixture_main
#include "political_shop_licensing_test.cpp"
#undef main
#include "../../src/resident/domains/politics/licensing/political_shop_member_history.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
using namespace hanmo::resident;
int main(){try{
 Fixture f;f.initialize();auto legal=f.context();const auto authority=f.authority();
 const auto response=[&](MemberResponse value){auto id=take(f.common.next_id("organization"));take(f.common.append_fact({id,"organization_member_response","organization"}));OrganizationFact fact;fact.id=id;fact.kind="organization_member_response";fact.organization_id="county";fact.actor_id="magistrate";fact.tick=f.common.tick();fact.response=value;f.organizations.facts.insert(id,fact);auto* member=f.organizations.members.find("magistrate.member");member->response=value;member->response_since_tick=f.common.tick();member->last_fact_id=id;return id;};
 const auto boundary=[&](){auto id=take(f.common.next_id("political_wanted"));take(f.common.append_fact({id,"political_wanted_registered","political_shop_licensing"}));return id;};
 auto early=boundary();auto refused=response(MemberResponse::refusing_new_duties);auto late=boundary();check(validate_political_shop_historical_member_duty(legal,authority,0,early).ok,"later same-tick refusal does not erase earlier signature");auto denied=validate_political_shop_historical_member_duty(legal,authority,0,late);check(!denied.ok&&denied.error.original.code=="PoliticalShopHistoricalMemberDutyDenied","same-tick refusal before signature rejects despite unchanged Office");
 auto office=validate_historical_office_signer(f.common,f.organizations,f.oc,authority,0,*organization_instance_sequence(late,"political_wanted"));check(office.ok,"fixture isolates member response from unchanged historical Office");
 auto resumed=response(MemberResponse::serving);auto later=boundary();check(validate_political_shop_historical_member_duty(legal,authority,0,later).ok,"actual later serving source enables only later boundary");check(!validate_political_shop_historical_member_duty(legal,authority,0,late).ok,"current serving cannot authorize old refused interval");
 const auto facts=f.common.state().facts;auto& rows=f.common.state().facts.rows();auto a=std::find_if(rows.begin(),rows.end(),[&](const auto& r){return r.first==refused;}),b=std::find_if(rows.begin(),rows.end(),[&](const auto& r){return r.first==late;});std::iter_swap(a,b);check(!validate_political_shop_historical_member_duty(legal,authority,0,late).ok,"canonical allocation and actual fact order must agree");f.common.state().facts=facts;
 auto member=*f.organizations.members.find("magistrate.member");f.organizations.members.find(member.id)->last_fact_id=refused;check(!validate_political_shop_historical_member_duty(legal,authority,0,later).ok,"terminal member response must match original latest source");f.organizations.members.set(member.id,member);
 auto catalog_member=*f.oc.members.find(member.id);f.oc.members.erase(member.id);check(!validate_political_shop_historical_member_duty(legal,authority,0,later).ok,"missing initial membership cannot be inferred from today's Office");f.oc.members.insert(member.id,catalog_member);
 check(!validate_political_shop_historical_member_duty(legal,authority,0,resumed).ok,"ordinary organization response is not a legal boundary capability");
 const auto common_before=encode_common_state(f.common.state());check(validate_political_shop_historical_member_duty(legal,authority,0,later).ok&&compare_boundary(common_before,encode_common_state(f.common.state())).exact(),"pure read does not allocate or alter money and state");
 std::cout<<checks<<" member source checks, 0 failures; typed source only\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

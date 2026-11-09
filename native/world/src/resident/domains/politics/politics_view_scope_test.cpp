// 单个本人邀请不能被后来的无关政治事实挤掉；只复现有限读来源问题。
#define main political_component_matrix_not_run
#include "politics_test.cpp"
#undef main
#include "politics_view_index.hpp"
#include "politics_fiscal.hpp"
int main(){Fixture f;SocialState social;auto offer=f.apply("lord",f.appoint());
    f.cc->organization_ids.push_back("remote.realm");f.cc->places.insert("remote.seat",{"remote.seat","county",{}});
    for(const auto& who:{StableId{"remote"},StableId{"unrelated"}}){auto actor=*f.common.state().actors.find("candidate");actor.id=who;actor.wallet_id=who+".wallet";actor.inventory_id=who+".inventory";actor.location=SettlementLocation{"county","remote.seat"};f.common.state().actors.insert(who,actor);f.common.state().wallets.insert(actor.wallet_id,{actor.wallet_id,{Owner::Kind::actor,who,{}},13,{}});f.common.state().inventories.insert(actor.inventory_id,{actor.inventory_id,{Owner::Kind::actor,who,{}},100,0,{}});}
    f.common.state().wallets.insert("remote.treasury",{"remote.treasury",{Owner::Kind::organization,"remote.realm",{}},100,{}});f.common.state().inventories.insert("remote.granary",{"remote.granary",{Owner::Kind::organization,"remote.realm",{}},100,0,{}});
    auto realm=*f.oc.organizations.find("realm");realm.id="remote.realm";realm.headquarters_site="remote.seat";realm.office_ids={"remote.office","remote.county"};f.oc.organizations.insert(realm.id,realm);
    auto issuer=*f.oc.offices.find("lord.office");issuer.id="remote.office";issuer.organization_id=realm.id;issuer.holder_actor_id="remote";issuer.grant_ids={"remote.grant"};f.oc.offices.insert(issuer.id,issuer);
    auto office=*f.oc.offices.find("county.office");office.id="remote.county";office.organization_id=realm.id;office.grant_ids={"remote.child"};f.oc.offices.insert(office.id,office);
    for(const auto& pair:{std::pair<StableId,StableId>{"lord.grant","remote.grant"},{"county.grant","remote.child"}}){auto grant=*f.oc.grants.find(pair.first);grant.id=pair.second;grant.office_id=pair.first=="lord.grant"?issuer.id:office.id;grant.scopes={{ScopeRef::Kind::site,"remote.seat"}};grant.wallet_ids={"remote.treasury"};grant.inventory_ids={"remote.granary"};f.oc.grants.insert(grant.id,grant);}
    auto member=*f.oc.members.find("candidate.member");member.id="unrelated.member";member.actor_id="unrelated";member.organization_id=realm.id;f.oc.members.insert(member.id,member);f.org=need(make_initial_organization(f.common,f.oc));
    auto district=*f.pc.jurisdictions.find("county");district.id="remote.county";district.seat_place_id="remote.seat";district.political_organization_id=district.controlling_organization_id=realm.id;district.office_id=office.id;district.public_wallet_id="remote.treasury";district.public_inventory_ids={"remote.granary"};f.pc.jurisdictions.insert(district.id,district);f.politics.jurisdictions.insert(district.id,district);
    // 150次真实远地任用/撤回共产300事实，本人并未收到它们；不插入伪fact。
    for(Integer n=0;n<150;++n){auto result=f.apply("remote",PoliticalAppointCommand{{realm.id,issuer.id,0,"remote"},district.id,"unrelated","appointment.policy",0,0});auto appointment=*f.politics.appointments.find(result.appointment_id);f.apply("remote",PoliticalWithdrawCommand{{realm.id,issuer.id,0,"remote"},appointment.id,appointment.revision});}
    f.valid();
    auto index=make_politics_view_index(f.common,f.politics,f.org,f.oc,social);
    auto view=need(query_politics_entries(f.common,*f.cc,f.politics,f.pc,f.org,f.oc,f.economy,social,f.activities,"candidate","capital",nullptr,&index));
    const auto& rows=boundary::array(boundary::field(view,"rows","view"),"rows");bool found=false;for(const auto& row:rows)if(boundary::get_string(row,"id","row")==offer.appointment_id)found=true;
    check(found,"one lawful active invitation survives 300 unrelated unknown facts");
    ++f.common.state().identity.revision;check(!query_politics_entries(f.common,*f.cc,f.politics,f.pc,f.org,f.oc,f.economy,social,f.activities,"candidate","capital",nullptr,&index).ok,"old identity index is rejected");--f.common.state().identity.revision;
    PoliticalFiscalState fiscal;PoliticalFiscalCatalog fiscal_catalog{true};
    f.oc.grants.find("lord.grant")->allowed_command_kinds.push_back("political_set_tax");f.oc.grants.find("remote.grant")->allowed_command_kinds.push_back("political_set_tax");
    ActorMutation location;location.actor_id="lord";location.location=SettlementLocation{"county","county.seat"};need(f.common.apply_actor(location));
    auto local_policy=need(apply_political_set_tax(f.common,f.politics,fiscal,fiscal_catalog,f.org,f.oc,f.activities,"lord",{{"realm","lord.office",0,"lord"},"county","public.granary",0,0,FiscalTaxMode::normal}));
    for(Integer n=0;n<300;++n)need(apply_political_set_tax(f.common,f.politics,fiscal,fiscal_catalog,f.org,f.oc,f.activities,"remote",{{"remote.realm","remote.office",0,"remote"},"remote.county","remote.granary",0,n,FiscalTaxMode::normal}));
    index=make_politics_view_index(f.common,f.politics,f.org,f.oc,social,&fiscal);
    auto taxes=need(query_political_fiscal_entries(f.common,*f.cc,f.politics,fiscal,fiscal_catalog,f.org,f.oc,f.activities,"lord","county.seat",&index));
    const auto& tax_rows=boundary::array(boundary::field(taxes,"rows","fiscal"),"rows");check(tax_rows.size()==1&&boundary::get_string(tax_rows.front(),"source_fact_id","fiscal")==local_policy.policy_fact_id,"300 unrelated policies cannot hide or replace own local policy");
    std::cout<<"PASS lawful invitation and local fiscal policy survive unrelated histories; stale source identity refused\n";
}

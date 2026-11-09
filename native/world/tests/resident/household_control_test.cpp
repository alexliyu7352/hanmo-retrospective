// 只核家庭权源与原子登记；不构造或借用旧ABI的WorldBundle。
#include "resident/domains/family/family_model.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace { int checks=0;void check(bool pass,const char* label){++checks;if(!pass)throw std::runtime_error(label);} }
int main(){try{
    auto shared=std::make_shared<CommonCatalog>();CommonState seed;seed.metadata.scenario_id="scenario.household";
    for(const auto id:{"manager","other"}){ActorRecord actor;actor.id=id;actor.alive=true;actor.available=true;actor.birth_tick=-30*720;seed.actors.insert(id,actor);shared->actor_initial_birth.insert(id,actor.birth_tick);}
    shared->places.insert("home",{"home","settlement",{}});RoleDefinition role;role.id="home.steward";role.initial_actor="manager";role.place_id="home";shared->roles.insert(role.id,role);seed.roles.insert(role.id,{"manager",true,{}});
    Owner owner;owner.kind=Owner::Kind::household;owner.id="household";seed.wallets.insert("cash",{"cash",owner,100,{},{}});seed.inventories.insert("food",{"food",owner,100,0,{},{}});
    CommonStore common(seed,shared);LocalEconomyState economy;economy.households.insert("household",{"household","cash","food"});FamilyState family;FamilyCatalog catalog;catalog.household_control.emplace();HouseholdControlDefinition d;d.id="mandate";d.actor_id="manager";d.household_group_id="household";d.source_role_id=role.id;d.through_tick=4;d.actor_accepted=true;catalog.household_control->initial_mandates.insert(d.id,d);
    check(initialize_household_control(common,family,*shared,economy,*catalog.household_control).ok,"initial accepted mandate");
    FamilyHouseholdControlAccess access(common,family,catalog,*shared,economy);auto proof=access.current("manager","household",household_provision_operation,0);check(proof.ok&&proof.value.has_value(),"actual source gives narrow authority");
    check(common.wallet("cash")->balance==100&&common.inventory("food")->lots.empty(),"authority does not move resources");
    auto stranger=access.current("other","household",household_provision_operation,0);check(stranger.ok&&!stranger.value,"same world actor has no implied authority");
    check(!access.current("manager","household","withdraw_private_cash",0).ok,"unsupported operation rejected");
    auto forged=*proof.value;forged.actor_id="other";check(!access.validate_accepted(forged,0).ok,"proof parties cannot be spliced");
    common.state().roles.find(role.id)->enabled=false;auto inactive=access.current("manager","household",household_provision_operation,0);check(inactive.ok&&!inactive.value&&access.validate_accepted(*proof.value,0).ok,"role loss blocks new but preserves accepted responsibility");
    common.state().identity.tick=5;common.state().actors.find("manager")->alive=false;auto ended=access.current("manager","household",household_provision_operation,5);check(ended.ok&&!ended.value&&access.validate_accepted(*proof.value,0).ok&&!access.validate_accepted(*proof.value,5).ok,"death and expiry do not invalidate earlier accepted scope");
    auto encoded=encode_household_control_fact(std::get<HouseholdControlFact>(family.facts.rows().front().second));auto decoded=decode_household_control_fact(encoded);FamilyState cold=family;cold.facts.rows().front().second=decoded;check(validate_household_control(common,cold,*shared,economy,&*catalog.household_control).ok,"cold fact retains source");
    std::get<HouseholdControlFact>(cold.facts.rows().front().second).through_tick=99;check(!validate_household_control(common,cold,*shared,economy,&*catalog.household_control).ok,"cold scope extension rejected");
    check(!validate_household_control(common,family,*shared,economy,nullptr).ok,"old profile rejects new source");
    const auto counter=common.state().next_instance_id;common.state().next_instance_id=1;check(!validate_household_control(common,family,*shared,economy,&*catalog.household_control).ok,"cold source cannot reuse an already assigned instance number");common.state().next_instance_id=counter;
    CommonStore failed(seed,shared);FamilyState untouched;failed.state().facts.insert("household_control:1",{"household_control:1","existing","other",{}});const auto next=failed.state().next_instance_id;auto result=initialize_household_control(failed,untouched,*shared,economy,*catalog.household_control);check(!result.ok&&untouched.facts.empty()&&failed.state().next_instance_id==next&&failed.state().facts.size()==1&&failed.wallet("cash")->balance==100,"failed registration rolls back sequence and does not write family or money");
    std::cout<<"HOUSEHOLD_CONTROL "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"HOUSEHOLD_CONTROL "<<checks<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}

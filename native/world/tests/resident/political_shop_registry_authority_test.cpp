// 接册底层签权/控制源门，复用原真实Office/Grant/完整初册；未冒军事占领或完整World。
#define main political_shop_licensing_component_main
#include "political_shop_licensing_test.cpp"
#undef main
#include "resident/domains/politics/licensing/political_shop_registry_detail.hpp"
using namespace hanmo::resident;
namespace {
struct RegistryFixture {
 Fixture legal;PoliticalTakeoverCatalog takeover_catalog;PoliticalTakeoverState takeover;MilitaryState military;MilitaryCatalog military_catalog;MilitaryAccess military_access;
 PoliticalShopRegistrySuccessionCatalog catalog;PoliticalShopRegistrySuccessionState state;
 RegistryFixture(){legal.initialize();takeover_catalog.enabled=true;catalog.enabled=true;PoliticalShopRegistrySuccessorPolicy policy{"court.successor","county.court","county","magistracy","court_counter","court_talk","licensing"};catalog.policies.insert(policy.id,policy);legal.oc.grants.find("law.grant")->allowed_command_kinds.push_back(political_shop_registry_accept_command);legal.common.state().roles.insert("court_counter",{"magistrate",true});}
};
struct MissingTakeoverRead:PoliticalTakeoverRead {
 Outcome<PoliticalTakeoverApproval> political_takeover_source(const StableId&)const override{return {false,{},common_failure("NoVerifiedTakeover").error};}
 Outcome<PoliticalResponsibilityExecution> political_responsibility_for_execution(const StableId&,const PoliticalResponsibilityRef&,const WorldIdentity&)const override{return {false,{},common_failure("NotUsed").error};}
 Outcome<std::optional<PoliticalResponsibilityExecution>> find_political_responsibility_for_execution(const PoliticalResponsibilityRef&,const WorldIdentity&)const override{return {false,{},common_failure("NotUsed").error};}
};
}
int main(){try{
 RegistryFixture f;auto law=f.legal.context();PoliticalShopControlContext control{f.legal.common,f.takeover_catalog,f.takeover,f.military,f.military_catalog,f.military_access};PoliticalShopRegistryContext x{law,control,f.catalog,f.state};
 check(validate_political_shop_registry_successions(x).ok,"empty explicit succession profile needs valid original complete court catalog");auto endpoint=take(shop_registry_detail::endpoint(x,*f.catalog.policies.find("court.successor")));check(take(shop_registry_detail::authority_now(x,endpoint,f.legal.authority()))=="law.grant","same real current judge needs separately declared accept grant");
 auto initial=take(read_political_shop_registry_control(control,f.legal.politics,f.legal.pc,*f.legal.catalog.courts.find("county.court")));check(initial.revision==0&&initial.registration_fact_id.empty(),"original control without occupation is not invented as succession");
 MissingTakeoverRead missing;PoliticalAcceptShopRegistry command{f.legal.authority(),"court.successor","missing.registration","missing.approval",{}};auto denied=preview_political_shop_registry_acceptance(x,missing,"magistrate",command);check(!denied.ok&&denied.error.original.code=="PoliticalShopRegistryApprovedControlRequired","current Office alone cannot accept an unoccupied book");
 f.legal.oc.grants.find("law.grant")->allowed_command_kinds.pop_back();check(!shop_registry_detail::authority_now(x,endpoint,f.legal.authority()).ok,"ordinary permit grant cannot accept old registry");f.legal.oc.grants.find("law.grant")->allowed_command_kinds.push_back(political_shop_registry_accept_command);
 f.legal.common.state().roles.find("court_counter")->actor_id="replacement";check(!shop_registry_detail::authority_now(x,endpoint,f.legal.authority()).ok,"accepting judge must actually staff declared counter");f.legal.common.state().roles.find("court_counter")->actor_id="magistrate";
 f.legal.politics.jurisdictions.find("county.jurisdiction")->revision=1;check(!read_political_shop_registry_control(control,f.legal.politics,f.legal.pc,*f.legal.catalog.courts.find("county.court")).ok,"changed jurisdiction without real military registration rejected");f.legal.politics.jurisdictions.find("county.jurisdiction")->revision=0;
 auto id=take(f.legal.common.next_id(political_shop_registry_accept_prefix));take(f.legal.common.append_fact({id,political_shop_registry_accept_kind,political_shop_registry_succession_module}));check(shop_registry_detail::authority_before(x,endpoint,f.legal.authority(),"law.grant",id,0).ok,"actual acceptance boundary supports exact original Office signer");check(!validate_political_shop_registry_successions(x).ok,"real head without acceptance payload does not pass full state");
 auto wrong=f.legal.authority();wrong.actor_id="replacement";check(!shop_registry_detail::authority_before(x,endpoint,wrong,"law.grant",id,0).ok,"historical signer cannot change to current or other actor");
 auto death_id=take(f.legal.common.next_id("military"));take(f.legal.common.append_fact({death_id,"military_casualties","military"}));FamilyDeathOrigin death{"magistrate",death_id,"actual.battle",0};law.death_origin=[death](const StableId& actor)->Outcome<std::optional<FamilyDeathOrigin>>{return {true,actor==death.actor_id?std::optional<FamilyDeathOrigin>{death}:std::nullopt,{}};};f.legal.common.state().actors.find("magistrate")->alive=false;
 check(shop_registry_detail::authority_before(x,endpoint,f.legal.authority(),"law.grant",id,0).ok,"later cause cannot erase earlier living acceptance signer");auto later=take(f.legal.common.next_id(political_shop_registry_accept_prefix));take(f.legal.common.append_fact({later,political_shop_registry_accept_kind,political_shop_registry_succession_module}));check(!shop_registry_detail::authority_before(x,endpoint,f.legal.authority(),"law.grant",later,0).ok,"cause before signing rejects before death registration");
 std::cout<<checks<<" registry authority checks, 0 failures; component source only\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

// 定点原关系和真实死亡前界；不代表完整自愿同意/换控或自然18年银行已经运行。
#include "resident/domains/family/voluntary_relation_source.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {Integer checks=0;void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}}
int main(){try{
 auto catalog=std::make_shared<CommonCatalog>();CommonState seed;seed.identity.world_id[0]=1;seed.identity.tick=4;seed.identity.revision=4;seed.next_instance_id=30;seed.metadata.player_actor_id="parent";seed.metadata.scenario_id="scene";FamilyState family;FamilyLineageCatalog lineage;lineage.enabled=true;lineage.endowment.families.insert("insight",{"insight",{"rites"}});
 for(const auto* id:{"parent","child","other"}){catalog->actor_ids.push_back(id);ActorRecord a;a.id=id;a.alive=a.available=true;a.birth_tick=StableId{id}=="parent"?-40*720:-20*720;catalog->actor_initial_birth.insert(id,*a.birth_tick);seed.actors.insert(id,a);}lineage.origins.insert("child",{"child",{"parent"}});
 AdultAdoptionFact adoption;adoption.agreement.adopter_actor_id="parent";adoption.agreement.adoptee_actor_id="child";adoption.agreement.accepted_tick=2;family.facts.insert("family:2",adoption);seed.facts.insert("family:2",{"family:2","family_adult_adoption","family"});seed.facts.insert("family:4",{"family:4","family_succession_consent","family"});seed.facts.insert("family_legacy:5",{"family_legacy:5","family_legacy_control_consent","family_legacy"});
 CommonStore c(seed,catalog);FamilyDeathOriginRead deaths=[](const StableId&)->Outcome<std::optional<FamilyDeathOrigin>>{return {true,{},{}};};VoluntaryRelationContext x{c,*catalog,family,&lineage,nullptr,nullptr,nullptr,deaths};const auto saved=encode_common_state(c.state());
 auto adopted=read_current_voluntary_relation(x,VoluntaryRelationKind::adoption,"parent","child","family:2");check(adopted.ok&&adopted.value.relation_fact_id=="family:2"&&!adopted.value.biological,"real adoption remains its exact source");
 check(read_voluntary_relation_before_fact(x,VoluntaryRelationKind::adoption,"parent","child","family:2",4,"family:4").ok,"prior adoption at actual consent");
 check(!read_current_voluntary_relation(x,VoluntaryRelationKind::adoption,"parent","other","family:2").ok,"adoption cannot change parties");
 check(!read_current_voluntary_relation(x,VoluntaryRelationKind::adoption,"parent","child").ok,"no latest adoption fallback");
 check(!read_voluntary_relation_before_fact(x,VoluntaryRelationKind::adoption,"parent","child","family:2",4,"family_legacy:5").ok,"posthumous consent cannot be voluntary boundary");
 auto biological=read_current_voluntary_relation(x,VoluntaryRelationKind::biological_child,"parent","child");check(biological.ok&&biological.value.biological&&biological.value.biological->kind==BiologicalChildOriginKind::installed_lineage&&biological.value.relation_fact_id.empty(),"Installed parentage never invents relation fact");
 check(!read_current_voluntary_relation(x,VoluntaryRelationKind::biological_child,"parent","child","family:2").ok,"social adoption not a biological source tag");
 check(!read_current_voluntary_relation(x,VoluntaryRelationKind::biological_child,"other","child").ok,"unknown parentage not inferred");
 check(!read_current_voluntary_relation(x,VoluntaryRelationKind::accepted_disciple,"parent","child","family:2").ok,"no apprenticeship source no qualification");
 check(!read_current_voluntary_relation(x,VoluntaryRelationKind::recognized_collateral,"parent","child","family:2").ok,"unrecognized ancestry not social recognition");
 auto absent=x;absent.death_origin={};check(!read_current_voluntary_relation(absent,VoluntaryRelationKind::adoption,"parent","child","family:2").ok,"current alive flag alone not sufficient source contract");
 check(compare_boundary(saved,encode_common_state(c.state())).exact(),"pure relation changed Common");
 c.state().actors.find("parent")->alive=false;c.state().facts.insert("military:8",{"military:8","military_casualties","military"});c.state().facts.insert("family:10",{"family:10","family_voluntary_succession","family"});x.death_origin=[&](const StableId& actor)->Outcome<std::optional<FamilyDeathOrigin>>{return actor=="parent"?Outcome<std::optional<FamilyDeathOrigin>>{true,FamilyDeathOrigin{actor,"military:8","battle",4},{}}:Outcome<std::optional<FamilyDeathOrigin>>{true,{},{}};};
 check(!read_current_voluntary_relation(x,VoluntaryRelationKind::adoption,"parent","child","family:2").ok,"dead predecessor cannot retire");
 check(read_voluntary_relation_before_fact(x,VoluntaryRelationKind::adoption,"parent","child","family:2",4,"family:4").ok,"later death preserves actual living consent");
 check(!read_voluntary_relation_before_fact(x,VoluntaryRelationKind::adoption,"parent","child","family:2",4,"family:10").ok,"death cause before completion rejects even without life registration");
 std::cout<<"VOLUNTARY_RELATION_SOURCE "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<checks<<" checks: "<<e.what()<<'\n';return 1;}}

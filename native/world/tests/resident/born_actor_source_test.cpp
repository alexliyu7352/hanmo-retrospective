// 新重建出生本人纯读必要门；定点typed输入不代十八年自然生育或真实会面World。
#include "resident/domains/family/born_actor_source.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {Integer checks=0;void check(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
struct Fixture {
 std::shared_ptr<CommonCatalog> catalog=std::make_shared<CommonCatalog>();CommonState initial;FamilyLineageCatalog lineage;FamilyBirthState births;FamilyDeathState life;
 Fixture(){initial.identity.world_id[0]=1;initial.identity.tick=18*720+3;initial.identity.revision=20;initial.metadata.scenario_id="scene";initial.next_instance_id=1000;catalog->actor_ids={"parent","mother","original_child","stranger"};catalog->places.insert("home",{"home","town",{}});lineage.enabled=true;lineage.endowment.families.insert("insight",{"insight",{"rites"}});
  for(const auto* id:{"parent","mother","original_child","stranger"}){ActorRecord a;a.id=id;a.alive=true;a.birth_tick=std::string(id)=="original_child"?-20*720:-40*720;initial.actors.insert(id,a);}lineage.origins.insert("original_child",{"original_child",{"parent","mother"}});
  initial.facts.insert("family_legacy:1",{"family_legacy:1","family_legacy_control_consent","family_legacy"});
 }
 // 只布原源所需条款和唯一身份，不伪装完整安排/育儿业务已经在World运行。
 void born(){catalog->family_birth_identity_enabled=true;BirthReply accepted;accepted.kind="family_birth_accepted";accepted.decision="accepted";accepted.due_tick=3;BirthArrangementSource source;source.speaker_actor_id="parent";source.listener_actor_id="mother";source.terms.child_prototype_id="newborn";source.terms.place_id="home";accepted.confirmation=source;births.replies.insert("family_birth:2",accepted);initial.facts.insert("family_birth:2",{"family_birth:2","family_birth_accepted","family_birth"});
  FamilyBirthRecord b;b.accepted_id="family_birth:2";b.actor_id="actor:5";b.prototype_id="newborn";b.display_name="child";b.place_id="home";b.tick=3;b.biological_parent_ids={"parent","mother"};auto innate=resolve_birth_innate_draw(lineage.endowment,"insight",{StableId{"parent"},{}},{StableId{"mother"},{}},0);check(innate.ok,"fixture innate original");b.innate.insert("insight",innate.value);births.births.insert("family_birth:4",b);initial.facts.insert("family_birth:4",{"family_birth:4","family_birth_recorded","family_birth"});
  ActorRecord a;a.id="actor:5";a.alive=true;a.birth_tick=a.created_tick=3;a.prototype_id="newborn";a.display_name="child";a.wallet_id="personal:actor:5";a.inventory_id="bag:actor:5";initial.actors.insert(a.id,a);initial.facts.insert(a.id,{a.id,"actor_creation","family_birth"});initial.facts.insert("family_legacy:20",{"family_legacy:20","family_legacy_control_consent","family_legacy"});
 }
};}
int main(){try{
 Fixture f;f.born();CommonStore c(f.initial,f.catalog);FamilyDeathOriginRead no_death=[](const StableId&)->Outcome<std::optional<FamilyDeathOrigin>>{return {true,{}, {}};};
 const auto unchanged=encode_common_state(c.state());auto read=[&](){return read_born_actor_source(c,*f.catalog,f.lineage,f.births,no_death,"actor:5");};
 auto born=read();check(born.ok&&born.value&&born.value->actor_id=="actor:5"&&born.value->birth_fact_id=="family_birth:4"&&born.value->creation_fact_id=="actor:5"&&born.value->prototype_id=="newborn","exact birth identity and prototype");
 check(born.value->biological_parent_ids==std::vector<StableId>{"parent","mother"}&&born.value->birth_tick==3&&born.value->adult&&born.value->alive,"actual parents and current adult qualification");
 auto authored=read_born_actor_source(c,*f.catalog,f.lineage,f.births,no_death,"parent");check(authored.ok&&!authored.value,"authored actor does not become a born instance");
 auto old=read_born_actor_source_before_fact(c,*f.catalog,f.lineage,f.births,no_death,"actor:5",3,"family_legacy:20");check(old.ok&&old.value&&!old.value->adult&&old.value->alive&&old.value->before_fact_id=="family_legacy:20","today adulthood not backfilled into actual birth-time contact");
 check(!read_born_actor_source_before_fact(c,*f.catalog,f.lineage,f.births,no_death,"actor:5",3,"family_legacy:1").ok,"same tick contact before creation rejected");
 check(!read_born_actor_source_before_fact(c,*f.catalog,f.lineage,f.births,no_death,"actor:5",3,"missing").ok,"nonexistent contact boundary rejected");
 f.catalog->family_birth_identity_enabled=false;check(!read().ok,"birth marker required");f.catalog->family_birth_identity_enabled=true;
 c.state().actors.find("actor:5")->birth_tick=2;check(!read().ok,"forged birthday rejected");c.state().actors.find("actor:5")->birth_tick=3;
 f.births.births.find("family_birth:4")->prototype_id="other";check(!read().ok,"forged prototype rejected");f.births.births.find("family_birth:4")->prototype_id="newborn";
 f.births.births.find("family_birth:4")->biological_parent_ids={"stranger","mother"};check(!read().ok,"forged parentage rejected");f.births.births.find("family_birth:4")->biological_parent_ids={"parent","mother"};
 FamilyBirthState missing;check(!read_born_actor_source(c,*f.catalog,f.lineage,missing,no_death,"actor:5").ok,"missing typed birth cannot be inferred from actor");
 check(compare_boundary(unchanged,encode_common_state(c.state())).exact(),"pure source read changed Common");
 c.state().actors.find("actor:5")->alive=false;c.state().facts.insert("military:40",{"military:40","military_casualties","military"});c.state().facts.insert("visit_reply:41",{"visit_reply:41","visit_message_replied","visit_messages"});
 FamilyDeathOriginRead death=[&](const StableId& actor)->Outcome<std::optional<FamilyDeathOrigin>>{if(actor=="actor:5")return {true,FamilyDeathOrigin{actor,"military:40","battle",c.tick()}, {}};return {true,{}, {}};};
 auto dead=read_born_actor_source(c,*f.catalog,f.lineage,f.births,death,"actor:5");check(dead.ok&&dead.value&&!dead.value->alive,"current real death retained");
 auto prior=read_born_actor_source_before_fact(c,*f.catalog,f.lineage,f.births,death,"actor:5",3,"family_legacy:20");check(prior.ok&&prior.value&&prior.value->alive,"later death does not erase earlier actual contact");
 auto after=read_born_actor_source_before_fact(c,*f.catalog,f.lineage,f.births,death,"actor:5",c.tick(),"visit_reply:41");check(after.ok&&after.value&&!after.value->alive,"same tick actual casualty before contact is dead");
 check(!read().ok,"today dead without original death source rejected");
 DynamicStepInput phase;phase.from_tick=c.tick();phase.to_tick=c.tick()+1;FamilyDeathOriginRead future_death=[&](const StableId& actor)->Outcome<std::optional<FamilyDeathOrigin>>{if(actor=="actor:5")return {true,FamilyDeathOrigin{actor,"military:40","battle",phase.to_tick},{}};return {true,{}, {}};};
 check(!read_born_actor_source_before_fact(c,*f.catalog,f.lineage,f.births,future_death,"actor:5",3,"family_legacy:20").ok,"old stable birth source still rejects a preclock future death");
 const auto unchanged_phase=encode_common_state(c.state());auto phase_before=read_born_actor_source_before_fact(c,*f.catalog,f.lineage,f.births,future_death,"actor:5",3,"family_legacy:20",phase);check(phase_before.ok&&phase_before.value&&phase_before.value->alive&&!phase_before.value->adult&&phase_before.value->observed_tick==3,"strict phase permits later actual death without erasing original birth-time meeting");
 check(!read_born_actor_source_before_fact(c,*f.catalog,f.lineage,f.births,future_death,"actor:5",phase.to_tick,"visit_reply:41",phase).ok,"phase cannot authorize future business boundary or new meeting");
 check(!read_born_actor_source_before_fact(c,*f.catalog,f.lineage,f.births,future_death,"actor:5",c.tick(),"visit_reply:41",phase).ok,"to death preceding alleged from-time contact is inconsistent");
 auto bad_phase=phase;++bad_phase.to_tick;check(!read_born_actor_source_before_fact(c,*f.catalog,f.lineage,f.births,future_death,"actor:5",3,"family_legacy:20",bad_phase).ok,"strict single half-day required");bad_phase=phase;--bad_phase.from_tick;check(!read_born_actor_source_before_fact(c,*f.catalog,f.lineage,f.births,future_death,"actor:5",3,"family_legacy:20",bad_phase).ok,"strict phase tied to actual Common clock");
 f.births.births.find("family_birth:4")->prototype_id="other";check(!read_born_actor_source_before_fact(c,*f.catalog,f.lineage,f.births,future_death,"actor:5",3,"family_legacy:20",phase).ok,"phase does not bypass original birth payload");f.births.births.find("family_birth:4")->prototype_id="newborn";
 check(compare_boundary(unchanged_phase,encode_common_state(c.state())).exact(),"strict phase historical source stays pure");
 std::cout<<"BORN_ACTOR_SOURCE "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

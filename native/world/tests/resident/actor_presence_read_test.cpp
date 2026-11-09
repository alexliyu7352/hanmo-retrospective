// 缺生日可会面、真实创建/死因前序及严格phase；不制造角色知情或代替完整生命周期总验。
#include "resident/domains/family/actor_presence_read.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}}
int main(){try{
 auto catalog=std::make_shared<CommonCatalog>();catalog->actor_ids={"original"};CommonState seed;seed.identity.tick=4;seed.next_instance_id=20;
 ActorRecord original;original.id="original";original.alive=true;seed.actors.insert(original.id,original);
 ActorRecord born;born.id="actor:7";born.alive=true;born.created_tick=3;seed.actors.insert(born.id,born);
 seed.facts.insert("appointment:9",{"appointment:9","personal_appointment_accepted","personal_appointments"});
 seed.facts.insert("actor:7",{"actor:7","actor_creation","family_birth"});
 seed.facts.insert("appointment:1",{"appointment:1","personal_appointment_started","personal_appointments"});
 CommonStore c(seed,catalog);FamilyDeathOriginRead none=[](const StableId&)->Outcome<std::optional<FamilyDeathOrigin>>{return {true,std::nullopt,{}};};
 auto old=read_actor_presence_before_fact(c,*catalog,none,"original",0,"appointment:9");check(old.ok&&old.value.alive,"authored person without birth date exists");
 check(!read_actor_presence_before_fact(c,*catalog,none,"actor:7",3,"appointment:9").ok,"current dynamic person cannot appear before actual creation");
 auto created=read_actor_presence_before_fact(c,*catalog,none,"actor:7",3,"appointment:1");check(created.ok&&created.value.creation_fact_id=="actor:7"&&created.value.alive,"actual ordered creation rather than numeric ID");
 check(!read_actor_presence_before_fact(c,*catalog,none,"actor:7",2,"appointment:1").ok,"created_tick cannot be shifted back");
 check(!read_actor_presence_before_fact(c,*catalog,none,"original",4,"missing").ok,"actual business boundary required");
 c.state().actors.find("actor:7")->alive=false;c.state().facts.insert("military:3",{"military:3","military_casualties","military"});
 c.state().facts.insert("appointment:2",{"appointment:2","personal_appointment_cancelled","personal_appointments"});
 FamilyDeathOriginRead death=[](const StableId& id)->Outcome<std::optional<FamilyDeathOrigin>>{return {true,FamilyDeathOrigin{id,"military:3","battle",4},{}};};
 auto prior=read_actor_presence_before_fact(c,*catalog,death,"actor:7",3,"appointment:1");check(prior.ok&&prior.value.alive&&prior.value.death_cause_fact_id.empty(),"later death does not erase prior lawful contact or reveal later death");
 auto dead=read_actor_presence_before_fact(c,*catalog,death,"actor:7",4,"appointment:2");check(dead.ok&&!dead.value.alive&&dead.value.death_cause_fact_id=="military:3","cause before later FamilyLife registration is already dead");
 check(!read_actor_presence(c,*catalog,none,"actor:7").ok,"today dead without source is unknown");
 c.state().identity.tick=3;DynamicStepInput phase;phase.from_tick=3;phase.to_tick=4;
 check(!read_actor_presence_before_fact(c,*catalog,death,"actor:7",3,"appointment:1").ok,"stable source rejects future cause");
 check(read_actor_presence_before_fact(c,*catalog,death,"actor:7",3,"appointment:1",&phase).value.alive,"strict phase can exclude later genuine cause while reading old contact");
 check(!read_actor_presence_before_fact(c,*catalog,death,"actor:7",4,"appointment:2",&phase).value.alive,"same phase sees genuine prior casualty");
 phase.to_tick=5;check(!read_actor_presence_before_fact(c,*catalog,death,"actor:7",3,"appointment:1",&phase).ok,"arbitrary future phase refused");
 c.state().identity.tick=4;c.state().facts.find("actor:7")->domain="wrong";check(!read_actor_presence(c,*catalog,death,"actor:7").ok,"unknown identity source not inferred from actor existence");
 std::cout<<"ACTOR_PRESENCE "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

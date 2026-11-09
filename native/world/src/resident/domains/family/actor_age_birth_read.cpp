// 年龄数学复用原720半日/年和18岁门槛；来源来自原出生，不取成熟原型或今日年龄推旧事实。
#include "actor_age_birth_read.hpp"
#include "family_birth.hpp"
#include <charconv>
namespace hanmo::resident {namespace {
bool allocated(const CommonStore& c,const StableId& id,const StableId& prefix){const auto lead=prefix+":";if(id.rfind(lead,0)!=0)return false;const auto digits=id.substr(lead.size());Integer n=0;auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),n);return parsed.ec==std::errc{}&&parsed.ptr==digits.data()+digits.size()&&n>0&&n<c.state().next_instance_id&&std::to_string(n)==digits;}
Outcome<ActorAgeRead> read(const CommonStore& c,const CommonCatalog& catalog,const FamilyBirthState& births,const StableId& actor,Integer tick,const StableId& boundary,bool historical){
 const auto* creation=c.state().facts.find(actor);const bool authored=std::find(catalog.actor_ids.begin(),catalog.actor_ids.end(),actor)!=catalog.actor_ids.end();if(authored||(creation&&creation->domain=="family"))return historical?read_actor_age_before_fact(c,catalog,actor,tick,boundary):read_actor_age(c,catalog,actor);
 auto fail=[&](const StableId& code)->Outcome<ActorAgeRead>{return {false,{},common_failure(code,actor).error};};constexpr Integer maximum_tick=2147483647,ticks_per_year=720;
 if(!catalog.family_birth_identity_enabled)return fail("ActorAgeBirthProfileRequired");const auto* person=c.state().actors.find(actor);if(!person||person->id!=actor)return fail("UnknownAgeActor");if(tick<0||tick>c.tick()||tick>maximum_tick||!person->birth_tick||*person->birth_tick<0||*person->birth_tick>tick||!person->created_tick||*person->created_tick!=*person->birth_tick||!person->prototype_id)return fail("ActorAgeBirthSourceRequired");
 const auto& facts=c.state().facts.order();const auto at=std::find(facts.begin(),facts.end(),actor);auto before=facts.end();if(historical){const auto* head=c.state().facts.find(boundary);before=std::find(facts.begin(),facts.end(),boundary);if(boundary.empty()||before==facts.end()||!head||head->id!=boundary)return fail("ActorAgeFactBoundaryRequired");}else if(!boundary.empty()||tick!=c.tick())return fail("ActorAgeCurrentBoundaryMismatch");
 if(!allocated(c,actor,"actor")||!creation||creation->id!=actor||creation->kind!="actor_creation"||creation->domain!="family_birth"||at==facts.end()||(historical&&at>=before))return fail("ActorAgeCreationHistoryRequired");
 const FamilyBirthRecord* original=nullptr;StableId birth_id;for(const auto& row:births.births.rows())if(row.second.actor_id==actor){if(original)return fail("ActorAgeBirthSourceAmbiguous");original=&row.second;birth_id=row.first;}
 const auto* head=c.state().facts.find(birth_id);const auto born=std::find(facts.begin(),facts.end(),birth_id);
 if(!original||!allocated(c,birth_id,"family_birth")||!head||head->id!=birth_id||head->kind!="family_birth_recorded"||head->domain!="family_birth"||original->kind!=head->kind||original->tick!=*person->birth_tick||original->tick!=*person->created_tick||original->prototype_id!=*person->prototype_id||born==facts.end()||born>=at||(historical&&born>=before))return fail("ActorAgeBirthSourceMismatch");
 const Integer years=(tick-original->tick)/ticks_per_year;return {true,{c.state().identity,actor,boundary,original->tick,tick,years,years>=18},{}};
}
}
Outcome<ActorAgeRead> read_actor_age(const CommonStore& c,const CommonCatalog& catalog,const FamilyBirthState& births,const StableId& actor){return read(c,catalog,births,actor,c.tick(),{},false);}
Outcome<ActorAgeRead> read_actor_age_before_fact(const CommonStore& c,const CommonCatalog& catalog,const FamilyBirthState& births,const StableId& actor,Integer tick,const StableId& boundary){return read(c,catalog,births,actor,tick,boundary,true);}
}

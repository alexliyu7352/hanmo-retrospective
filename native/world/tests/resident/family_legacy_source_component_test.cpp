// 遗留来源纯读必要门；夹具只核关系和终止引用，不表示正式军战或本人同意已发生。
#include "resident/domains/family/family_legacy_source.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
// Release下保留精确首错。
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
// 使用同一Common人物与实际Family variant，避免在纯读接口另建亲缘或余额账。
struct Fixture {
    std::shared_ptr<CommonCatalog> catalog=std::make_shared<CommonCatalog>();CommonState common;FamilyState family;FamilyDeathState deaths;
    Fixture(){
        common.identity.world_id[0]=7;common.identity.tick=10;common.identity.revision=5;common.metadata.scenario_id="scene";common.metadata.player_actor_id="predecessor";common.next_instance_id=4;
        ActorRecord actor;actor.id="predecessor";actor.alive=false;actor.available=false;actor.hp=0;actor.birth_tick=-30000;common.actors.insert(actor.id,actor);
        actor.id="successor";actor.alive=true;actor.available=true;actor.hp=70;actor.birth_tick=-20000;actor.action_id="real_ongoing_action";common.actors.insert(actor.id,actor);
        common.facts.insert("adoption:1",{"adoption:1","family_adult_adoption","family",{}});
        common.facts.insert("military:2",{"military:2","military_casualties","military",{}});
        common.facts.insert("family_death:3",{"family_death:3","family_death","family_life",{}});
        AdultAdoptionFact adoption;adoption.agreement.adopter_actor_id="predecessor";adoption.agreement.adoptee_actor_id="successor";adoption.agreement.accepted_tick=4;family.children.insert("successor","adoption:1");family.facts.insert("adoption:1",adoption);
        FamilyDeathRecord death;death.actor_id="predecessor";death.scenario_id="scene";death.cause_fact_id="military:2";death.battle_id="battle:original";death.world_id=common.identity.world_id;death.death_tick=8;death.recorded_tick=9;deaths.deaths.insert("family_death:3",death);
    }
    // 仅模拟已完成军事验证后的具名只读投影，不从alive标记生成证明。
    FamilyDeathOriginRead source()const{return [](const StableId& id)->Outcome<std::optional<FamilyDeathOrigin>>{if(id=="predecessor")return {true,FamilyDeathOrigin{id,"military:2","battle:original",8},{}};return {true,std::nullopt,{}};};}
};
}
// 候选忙碌不改变原行动；返回依据仍不是可操作资格、接受事实或继产许可。
int main(){try{
    Fixture f;CommonStore common(f.common,f.catalog);const auto before=encode_common_state(common.state());auto read=[&](){return read_family_legacy_adoption_basis(common,f.family,f.deaths,f.source(),"predecessor","successor");};
    auto basis=read();check(basis.ok&&basis.value.death_fact_id=="family_death:3"&&basis.value.casualty_fact_id=="military:2"&&basis.value.relation_fact_id=="adoption:1"&&basis.value.successor_birth_tick==-20000,"exact living relation and death source lost");
    check(compare_boundary(before,encode_common_state(common.state())).exact()&&common.actor("successor")->action_id=="real_ongoing_action","source query changed control/resources/action or RNG");
    auto saved_deaths=f.deaths;f.deaths=FamilyDeathState{};check(!read().ok,"missing death payload permitted basis");f.deaths=saved_deaths;
    f.deaths.deaths.rows().front().second.cause_fact_id="military:wrong";check(!read().ok,"forged death cause permitted basis");f.deaths=saved_deaths;
    f.family.children.set("successor","missing");check(!read().ok,"bare child identity permitted relation");f.family.children.set("successor","adoption:1");
    common.state().actors.find("successor")->birth_tick=0;auto young=read();check(!young.ok&&young.error.original.code=="SuccessionRequiresAdult","minor entered adult continuation basis");common.state().actors.find("successor")->birth_tick=-20000;
    common.state().actors.find("successor")->alive=false;check(!read().ok,"dead successor entered living continuation basis");common.state().actors.find("successor")->alive=true;
    auto& adoption=std::get<AdultAdoptionFact>(*f.family.facts.find("adoption:1"));adoption.agreement.accepted_tick=9;check(!read().ok,"post-death relationship permitted basis");adoption.agreement.accepted_tick=4;
    auto header=*common.state().facts.find("adoption:1");common.state().facts.erase("adoption:1");common.state().facts.insert("adoption:1",header);auto reordered=read();check(!reordered.ok&&reordered.error.original.code=="LegacyRelationshipAfterDeath","same-tick late relationship passed causal ordering");
    std::cout<<"FAMILY_LEGACY_SOURCE "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAMILY_LEGACY_SOURCE "<<checks<<" checks 1 failure: "<<error.what()<<'\n';return 1;}}

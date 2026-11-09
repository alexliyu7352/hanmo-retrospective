// 实际原close应用加永久边界；typed最终战果夹具不冒完整战术回放或World。
#include "resident/domains/military/military_recovery_release.hpp"
#include "resident/domains/military/military_recovery_codec.hpp"
#include "resident/domains/military/battle_world.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}template<class T>T take(Outcome<T> r){if(!r.ok)throw std::runtime_error(r.error.original.code);return std::move(r.value);}void good(Status r){if(!r.ok)throw std::runtime_error(r.error.original.code);}
}
int main(){try{
 CommonState seed;seed.identity.tick=5;seed.next_instance_id=6;ActorRecord actor;actor.id="hero";actor.alive=actor.available=true;actor.hp=60;seed.actors.insert(actor.id,actor);seed.facts.insert("battle:4",{"battle:4","military_battle","military"});seed.facts.insert("military:5",{"military:5","military_casualties","military"});CommonStore c(seed,std::make_shared<CommonCatalog>());MilitaryCatalog cat;cat.recovery_policy=MilitaryRecoveryPolicy{};MilitaryState m;MilitaryBattleRecord b;b.id="battle:4";b.controller_actor_id="hero";b.phase="result";b.applied=true;b.completed_tick=5;b.settlement_receipt_id="military:5";m.battles={b};MilitaryBattleFact result;result.id=b.id;m.battle_facts={result};MilitaryCasualtyFact settled;settled.id="military:5";settled.battle_id=b.id;settled.tick=5;m.casualty_facts={settled};MilitaryAccess access;access.player_actor_id="hero";access.fact_exists=[&](const StableId& id){return c.state().facts.find(id)!=nullptr;};MilitaryBattleClose close;close.battle_id=b.id;
 auto effect=take(apply_battle_command(c,m,cat,access,"hero",MilitaryCommand{close}));check(effect.battle_id==b.id&&m.battles[0].phase=="closed","original close actually commits");check(m.recovery.battle_releases.size()==1&&m.recovery.battle_releases[0].tick==5,"same candidate writes exactly one release");good(validate_military_battle_release_history(c,m,cat));check(!apply_battle_command(c,m,cat,access,"hero",MilitaryCommand{close}).ok,"original completed battle rejects second close");auto bytes=military_recovery_codec::encode(m.recovery);auto decoded=take(military_recovery_codec::decode(bytes));check(decoded.battle_releases.size()==1&&decoded.battle_releases[0].settlement_receipt_id==settled.id,"strict codec preserves actual casualty source");
 const auto saved=m;m.recovery.battle_releases[0].actor_id="other";check(!validate_military_battle_release_history(c,m,cat).ok,"unrelated controller cannot supply release");m=saved;auto original=c;const auto id=m.recovery.battle_releases[0].id;c.state().facts.erase(id);m.recovery.battle_releases.clear();check(!validate_military_battle_release_history(c,m,cat).ok,"manual closed state without original source rejected");m=saved;c=original;auto old_cat=cat;old_cat.recovery_policy.reset();check(!validate_military_battle_release_history(c,m,old_cat).ok,"old profile rejects new release");
 m.battles[0].phase="result";m.recovery.battle_releases.clear();c.state().facts.erase(id);c.state().next_instance_id=std::numeric_limits<Integer>::max();check(!apply_battle_command(c,m,cat,access,"hero",MilitaryCommand{close}).ok&&m.battles[0].phase=="result"&&m.recovery.battle_releases.empty(),"failed source allocation cannot close battle");
 std::cout<<"MILITARY_RECOVERY_RELEASE="<<checks<<" FAILURES=0 SCOPE=ORIGINAL_COMMAND_COMPONENT\n";return 0;
 }catch(const std::exception& e){std::cerr<<"MILITARY_RECOVERY_RELEASE_FAIL "<<checks<<" "<<e.what()<<'\n';return 1;}}

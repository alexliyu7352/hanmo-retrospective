// 只验新纯整数公式与人数上限顺序，不代替真实军权、XP历史、完整World或旧档。
#include "resident/domains/military/military_commanded_damage.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class T>T take(Outcome<T> r){if(!r.ok)throw std::runtime_error(r.error.original.code);return std::move(r.value);}
}
int main(){try{
 check(take(military_commanded_strength(1,0,0))==35,"minimum");
 check(take(military_commanded_strength(110,4,100))==130,"maximum");
 check(take(military_commanded_strength(59,2,19))==77,"independent floors");
 auto q=take(military_commanded_damage_base(500,500,60,2,50,60,2,50));
 check(q.attack_strength==85&&q.defense_strength==85&&q.base_casualties==38,"original formula");
 check(take(military_commanded_damage_base(1,1,1,0,0,110,4,100)).base_casualties==1,"minimum one");
 check(take(military_commanded_damage_base(500,1,100,4,100,1,0,0)).base_casualties==77,"target cap follows complete factors");
 check(!military_commanded_strength(0,0,0).ok&&!military_commanded_strength(111,0,0).ok,"leadership range");
 check(!military_commanded_strength(1,5,0).ok&&!military_commanded_strength(1,0,101).ok,"skill and training range");
 check(!military_commanded_damage_base(501,1,1,0,0,1,0,0).ok&&!military_commanded_damage_base(1,0,1,0,0,1,0,0).ok,"actual nonempty roster");
 check(take(military_commanded_skill("archer"))=="archery"&&take(military_commanded_skill("cavalry"))=="cavalry"&&take(military_commanded_skill("infantry"))=="infantry","explicit troop mapping");
 check(!military_commanded_skill("ship").ok,"unknown skill is not default");
 std::cout<<"MILITARY_COMMANDED_DAMAGE_FORMULA="<<checks<<" FAILURES=0 SCOPE=PURE_FORMULA\n";return 0;
}catch(const std::exception& e){std::cerr<<"MILITARY_COMMANDED_DAMAGE_FORMULA_FAIL "<<checks<<" "<<e.what()<<'\n';return 1;}}

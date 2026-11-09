// 共用原Family准备门：结束会话、原行动/受托责任与已退隐都先于换控。
#include "resident/domains/family/voluntary_family_readiness.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace{int checks=0;void check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}}
int main(){try{CommonState seed;auto catalog=std::make_shared<CommonCatalog>();for(const auto* id:{"first","second"}){ActorRecord a;a.id=id;a.alive=a.available=true;a.hp=100;seed.actors.insert(id,a);}CommonStore c(seed,catalog);FamilyState family;EstateState estate;InteractionState interaction;InteractionAccess ia;TaskState tasks;FamilyAccess access;access.interaction=&interaction;access.interaction_access=&ia;access.tasks=&tasks;bool original_retire=true;access.military_or_office_pending=[&](const StableId&,bool retire){original_retire=retire;return false;};
 auto ready=[&]{return validate_voluntary_family_parties_ready(c,family,estate,access,"first","second");};check(ready().ok&&!original_retire,"original voluntary scope does not transfer office");
 c.state().actors.find("second")->action_id="action:1";check(!ready().ok&&ready().error.original.code=="ActorBusy","candidate action blocks");c.state().actors.find("second")->action_id.clear();
 c.state().actors.find("first")->injury=3;check(!ready().ok,"severe injury blocks original actor");c.state().actors.find("first")->injury=1;check(ready().ok,"original minor injury rule preserved");
 InteractionSession talk;talk.actor_id="first";talk.context.provider_actor_id="second";interaction.interactions.push_back(talk);check(!ready().ok&&ready().error.original.code=="FamilyConversationPending","must finish the actual consent dialogue");interaction.interactions.clear();
 access.tasks=nullptr;check(!ready().ok,"missing original responsibility source rejects");access.tasks=&tasks;
 ia.battle_occupied=[](const StableId& a){return a=="second";};check(!ready().ok&&ready().error.original.code=="BattlePending","original actual battle occupancy blocks");ia.battle_occupied={};
 access.military_or_office_pending=[](const StableId& a,bool){return a=="first";};check(!ready().ok,"original military readiness blocks");access.military_or_office_pending=[](const StableId&,bool){return false;};
 SuccessionConsentFact plan;plan.agreement.personal.predecessor_actor_id="second";plan.agreement.personal.successor_actor_id="third";plan.conclusion=SuccessionPlanConclusion{"completed",0,"family:2",{}};family.facts.insert("family:1",plan);family.facts.insert("family:2",VoluntarySuccessionFact{"family_voluntary_succession","family:1",0,{}});family.successions.push_back("family:2");check(!ready().ok&&ready().error.original.code=="FamilyAlreadyRetired","original retired successor excluded");
 std::cout<<"VOLUNTARY_FAMILY_READINESS "<<checks<<" checks 0 failures\n";return 0;}catch(const std::exception& e){std::cerr<<checks<<" checks: "<<e.what()<<'\n';return 1;}}

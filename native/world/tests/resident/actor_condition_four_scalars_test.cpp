// 原Common四身体标量瞬态捕获与coverage反验；不建立永久身体表或授任何业务原因。
#include "resident/storage/actor_condition_execution_coverage.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
Integer checks=0;
// 一项失败即保留真实断点，不以数量替代集成结果。
void need(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
// 只搭原Common writer必要人物，不伪造其业务cause或Origin来源。
CommonStore fixture(){auto catalog=std::make_shared<CommonCatalog>();catalog->actor_ids={"actor"};CommonState state;ActorRecord actor;actor.id="actor";actor.alive=true;actor.hp=80;actor.fatigue=20;actor.injury=1;state.actors.insert(actor.id,actor);return CommonStore(std::move(state),catalog);}
}
int main(){try{
 auto common=fixture(),plain=fixture();auto begun=begin_actor_condition_coverage(common);need(begun.ok,"begin complete body coverage");
 ActorMutation mutation;mutation.actor_id="actor";mutation.injury=1;need(common.apply_actor(mutation).ok&&plain.apply_actor(mutation).ok,"same-value injury writer");
 const auto& injury=common.actor_condition_execution_drafts().back();need(injury.injury_written&&!injury.alive_written&&!injury.hp_written&&!injury.fatigue_written&&injury.before->injury==1&&injury.after.injury==1&&injury.before->alive&&injury.after.alive,"injury same-value actual presence");
 mutation={};mutation.actor_id="actor";mutation.alive=true;need(common.apply_actor(mutation).ok&&plain.apply_actor(mutation).ok,"same-value alive writer");
 const auto& alive=common.actor_condition_execution_drafts().back();need(alive.alive_written&&!alive.injury_written&&alive.before->alive&&alive.after.alive,"alive same-value actual presence");
 mutation.alive=false;mutation.injury=0;need(common.apply_actor(mutation).ok&&plain.apply_actor(mutation).ok,"false alive and zero injury writes");
 const auto& zero=common.actor_condition_execution_drafts().back();need(zero.alive_written&&zero.injury_written&&!zero.after.alive&&zero.after.injury==0,"false and zero cannot look absent");
 need(compare_boundary(encode_common_state(common.state()),encode_common_state(plain.state())).exact(),"capture leaves original common serialization unchanged");auto completed=finish_actor_condition_coverage(common,begun.value);need(completed.ok&&completed.value.size()==3,"complete four-scalar coverage accepted");
 for(bool life:{false,true}){auto direct=fixture();auto boundary=begin_actor_condition_coverage(direct);need(boundary.ok,"raw bypass boundary");if(life)direct.state().actors.find("actor")->alive=false;else direct.state().actors.find("actor")->injury=0;need(!finish_actor_condition_coverage(direct,boundary.value).ok,"direct injury or alive state change rejected");}
 for(bool life:{false,true}){auto forged=fixture();auto boundary=begin_actor_condition_coverage(forged);need(boundary.ok,"presence tamper boundary");ActorMutation m;m.actor_id="actor";if(life)m.alive=false;else m.injury=0;need(forged.apply_actor(m).ok,"actual body mutation before tamper");auto& rows=const_cast<std::vector<ActorConditionExecutionDraft>&>(forged.actor_condition_execution_drafts());if(life)rows.back().alive_written=false;else rows.back().injury_written=false;need(!finish_actor_condition_coverage(forged,boundary.value).ok,"removed actual presence rejected");}
 auto rollback=fixture();need(rollback.begin_actor_condition_capture().ok,"rollback capture");auto point=rollback.begin_savepoint();need(point.ok,"rollback savepoint");ActorMutation m;m.actor_id="actor";m.injury=0;m.alive=false;need(rollback.apply_actor(m).ok,"rollback four-scalar write");need(rollback.rollback_savepoint(point.value).ok&&rollback.actor("actor")->alive&&rollback.state().actors.find("actor")->injury==1&&rollback.actor_condition_execution_drafts().empty(),"original rollback restores fields and capture prefix");need(rollback.finish_actor_condition_capture().ok,"rollback capture closes");
 std::cout<<"ACTOR_CONDITION_FOUR_SCALARS "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& error){std::cerr<<"ACTOR_CONDITION_FOUR_SCALARS "<<checks<<" checks 1 failure: "<<error.what()<<'\n';return 1;}}

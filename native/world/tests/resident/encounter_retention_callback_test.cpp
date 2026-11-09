// 实测原安装回调可在同一retained向量中固定真实新ID；不新增API、永久史或运输许可。
#include "resident/domains/interaction/interaction_model.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
// 失败即保原诊断退出，不把未执行的后续断言记通过。
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
// 全部128个旧路遇都由原install创建，后续编号增长仍走原Common分配器。
struct Fixture {
 CommonStore common;
 InteractionState interaction;
 std::vector<StableId> retained;
 Fixture():common(CommonState{},std::make_shared<CommonCatalog>()){
  common.state().next_instance_id=1;
  for(const auto* id:{"a","b","player"}){ActorRecord a;a.id=id;a.alive=true;a.available=true;a.hp=100;a.location=SettlementLocation{"town","start"};common.state().actors.set(a.id,a);}
  while(common.state().next_instance_id<200)if(!common.next_id("unused").ok)throw std::runtime_error("original ID allocation failed");
  for(int i=0;i<128;++i){auto result=install(false,false,"player");if(!result.ok)throw std::runtime_error("original old encounter failed");}
  check(interaction.encounters.size()==128,"old resolved cache exactly full");
  while(common.state().next_instance_id<1000)if(!common.next_id("unused").ok)throw std::runtime_error("original ID growth failed");
 }
 // 只在原运动写者成功之后、安装返回之前保真实本次ID；失败不造来源或pin。
 Outcome<StableId> install(bool keep,bool fail,const StableId& player){
  EncounterInstall hit;hit.leaders={"a","b"};hit.anchors={Location{SettlementLocation{"town","contact"}},Location{SettlementLocation{"town","contact"}}};
  hit.apply_motion=[&,keep,fail](const StableId& actual,bool interactive){
   for(const auto* id:{"a","b"}){ActorMutation m;m.actor_id=id;m.location=SettlementLocation{"town","contact"};auto v=common.apply_actor(m);if(!v.ok)return v;}
   if(fail)return common_failure("InjectedMotionFailure");
   if(keep)retained.push_back(actual);
   return common_success();
  };
  return install_encounter(common,interaction,player,hit,retained);
 }
 // 查询原安装后的实际缓存，不把回调收到ID等同于已留存。
 bool has(const StableId& id)const{return std::any_of(interaction.encounters.begin(),interaction.encounters.end(),[&](const auto& e){return e.id==id;});}
};
}
// 同原安装链核新ID裁剪、同步引用及候选失败回滚。
int main(){try{
 Fixture plain;auto ordinary=plain.install(false,false,"player");check(ordinary.ok&&ordinary.value=="encounter:1000","actual next ID at wider decimal boundary");
 check(!plain.has(ordinary.value)&&plain.interaction.encounters.size()==128,"unreferenced new ID can be lexicographically oldest and trimmed");
 Fixture kept;auto actual=kept.install(true,false,"player");check(actual.ok&&actual.value==ordinary.value,"same original allocation with retention");
 check(kept.retained==std::vector<StableId>{actual.value}&&kept.has(actual.value),"same callback vector protects exact new ID before trim");
 check(kept.interaction.encounters.size()==129,"one referenced record excluded from original128 quota");
 auto next=kept.install(true,false,"player");check(next.ok&&kept.has(actual.value)&&kept.has(next.value)&&kept.interaction.encounters.size()==130,"existing and new exact references survive next same-tick install");
 Fixture pending;auto pending_id=pending.install(false,false,"a");check(pending_id.ok&&pending.has(pending_id.value)&&pending.interaction.encounters.size()==129,"interactive pending stays outside resolved trimming without a pin");
 Fixture failure;const auto common_before=encode_common_state(failure.common.state());const auto size=failure.interaction.encounters.size();auto point=failure.common.begin_savepoint();check(point.ok,"original candidate savepoint opens");
 auto rejected=failure.install(true,true,"player");check(!rejected.ok&&rejected.error.original.code=="InjectedMotionFailure","original callback failure returned unchanged");
 check(failure.retained.empty()&&failure.interaction.encounters.size()==size,"failed motion neither installs nor captures new ID");
 check(failure.common.rollback_savepoint(point.value).ok&&compare_boundary(common_before,encode_common_state(failure.common.state())).exact(),"original outer savepoint restores allocated IDs and actual motion");
 std::cout<<"ENCOUNTER_RETENTION_CALLBACK="<<checks<<" FAILURES=0 SCOPE=ORIGINAL_INSTALL_ONLY\n";return 0;
}catch(const std::exception& e){std::cerr<<"checks="<<checks<<" "<<e.what()<<'\n';return 1;}}

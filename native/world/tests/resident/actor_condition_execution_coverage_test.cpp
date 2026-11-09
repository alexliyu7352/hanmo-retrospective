// 真正Common人物写回与末态的必要覆盖；不是完整业务史、授权或World测试。
#include "resident/storage/actor_condition_execution_coverage.hpp"
#include "resident/storage/common_store.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
// 首个必要边界失败立即保原红，输出数只属于本组件。
void need(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
// 创建仅借Common现有资产身份门；不捏造Family/Installed创建来源。
ActorRecord newborn(){ActorRecord a;a.id="new";a.hp=100;a.fatigue=0;a.wallet_id="wnew";a.inventory_id="inew";return a;}
// 原真实人物和专属资产供局部writer运行，外层World初态资格不在此夹具。
CommonStore fixture(){auto cat=std::make_shared<CommonCatalog>();CommonState state;state.identity.tick=3;state.identity.revision=8;state.next_instance_id=10;
 for(const auto& id:{StableId{"p"},StableId{"q"}}){ActorRecord a;a.id=id;a.hp=100;a.fatigue=0;a.wallet_id="w"+id;a.inventory_id="i"+id;state.actors.insert(id,a);cat->actor_ids.push_back(id);}
 state.wallets.insert("wnew",{"wnew",{Owner::Kind::actor,"new",{},{}},0,{},{}});state.inventories.insert("inew",{"inew",{Owner::Kind::actor,"new",{},{}},10,0,{},{}});return CommonStore(std::move(state),std::move(cat));}
// 原世界与未发布候选独立，失败不向调用方交付任何草稿且不偷偷修复可疑末值。
void rejects(const std::function<void(CommonStore&)>& edit){auto original=fixture();CommonStore candidate(original);auto bound=begin_actor_condition_coverage(candidate);need(bound.ok,"begin negative candidate");edit(candidate);auto result=finish_actor_condition_coverage(candidate,bound.value);need(!result.ok&&result.value.empty(),"gap produced usable drafts");need(original.actor("p")->hp==100&&original.actor("q")->fatigue==0&&original.state().actors.size()==2,"candidate altered original");}
// 只篡改单份本组件原capture来验证拒门，真实调用者没有可写草稿接口。
void rejects_frame(const std::function<void(ActorConditionExecutionDraft&)>& edit){rejects([&](auto& c){ActorMutation m;m.actor_id="p";m.hp=80;need(c.apply_actor(m).ok,"frame original write");auto& rows=const_cast<std::vector<ActorConditionExecutionDraft>&>(c.actor_condition_execution_drafts());edit(rows.front());});}
}
int main(){try{
 auto c=fixture();auto b=begin_actor_condition_coverage(c);need(b.ok,"begin actual");need(!begin_actor_condition_coverage(c).ok,"nested capture");ActorMutation m;m.actor_id="p";m.hp=100;m.fatigue=0;need(c.apply_actor(m).ok,"same-value actual write");m.hp=0;m.fatigue.reset();need(c.apply_actor(m).ok,"zero HP writer");need(c.next_id("actual_business").ok,"original domain ID gap");need(c.create_actor(newborn(),0).ok,"actual dynamic creation");m.actor_id="new";m.hp.reset();m.fatigue=7;need(c.apply_actor(m).ok,"created actor mutation");auto copy=b.value;auto out=finish_actor_condition_coverage(c,copy);need(out.ok&&out.value.size()==4&&out.value[0].hp_written&&out.value[1].after.hp==0&&!out.value[2].before,"actual captured chain rejected");need(!finish_actor_condition_coverage(c,b.value).ok,"copied boundary consumed twice");
 rejects([](auto& c){c.state().actors.find("p")->hp=50;});
 rejects([](auto& c){c.state().actors.find("q")->fatigue=3;});
 rejects([](auto& c){c.state().actors.erase("q");});
 rejects([](auto& c){c.state().actors.insert("new",newborn());});
 rejects([](auto& c){auto q=*c.state().actors.find("p");c.state().actors.erase("p");c.state().actors.insert("p",q);});
 rejects([](auto& c){c.state().actors.find("p")->id="q";});
 rejects([](auto& c){c.state().next_instance_id--;});
 rejects([](auto& c){c.state().identity.world_id[0]=1;});
 rejects([](auto& c){c.state().identity.execution_epoch++;});
 // 直接写被后来真实写恢复净值时，记录前像仍能指出缺口。
 rejects([](auto& c){c.state().actors.find("p")->hp=50;ActorMutation m;m.actor_id="p";m.hp=100;need(c.apply_actor(m).ok,"compensating actor write");});
 rejects([](auto& c){ActorMutation m;m.actor_id="p";m.hp=80;c.state().identity.tick=5;need(c.apply_actor(m).ok,"future write");c.state().identity.tick=4;m.hp=100;need(c.apply_actor(m).ok,"past write");c.state().identity.tick=5;});
 rejects_frame([](auto& f){f.sequence=2;});rejects_frame([](auto& f){f.actor_id="q";});rejects_frame([](auto& f){f.before->hp--;});rejects_frame([](auto& f){f.hp_written=false;});rejects_frame([](auto& f){f.before.reset();});rejects_frame([](auto& f){f.kind=ActorConditionExecutionDraft::Kind::creation;});rejects_frame([](auto& f){f.kind=static_cast<ActorConditionExecutionDraft::Kind>(5);});rejects_frame([](auto& f){f.next_instance_after++;});rejects_frame([](auto& f){f.after.fatigue++;});
 auto rolled=fixture();auto rb=begin_actor_condition_coverage(rolled);need(rb.ok,"rollback begin");auto point=rolled.begin_savepoint();need(point.ok,"rollback point");m={};m.actor_id="p";m.hp=60;need(rolled.apply_actor(m).ok&&rolled.create_actor(newborn(),0).ok,"rollback actual actor writes");need(!finish_actor_condition_coverage(rolled,rb.value).ok,"open savepoint stole capture");need(rolled.rollback_savepoint(point.value).ok,"original rollback");auto undone=finish_actor_condition_coverage(rolled,rb.value);need(undone.ok&&undone.value.empty()&&rolled.actor("p")->hp==100&&!rolled.actor("new"),"rollback prefix rejected");
 auto committed=fixture();auto cb=begin_actor_condition_coverage(committed);need(cb.ok,"commit begin");auto cp=committed.begin_savepoint();need(cp.ok&&committed.apply_actor(m).ok&&committed.commit_savepoint(cp.value).ok,"actual commit");need(finish_actor_condition_coverage(committed,cb.value).ok,"commit capture rejected");
 auto other=fixture();auto ob=begin_actor_condition_coverage(other);need(ob.ok,"cross begin");CommonStore clone(other);need(!finish_actor_condition_coverage(clone,ob.value).ok,"clone consumed source");need(finish_actor_condition_coverage(other,ob.value).ok,"clone attempt stole source");
 auto reopened=fixture();auto old=begin_actor_condition_coverage(reopened);need(old.ok,"reopen begin");need(reopened.finish_actor_condition_capture().ok&&reopened.begin_actor_condition_capture().ok,"raw reopen");need(!finish_actor_condition_coverage(reopened,old.value).ok,"old boundary accepted new identity");need(reopened.finish_actor_condition_capture().ok,"stale boundary stole new capture");
 auto moved=fixture();auto mb=begin_actor_condition_coverage(moved);need(mb.ok,"move begin");CommonStore target(std::move(moved));need(!finish_actor_condition_coverage(target,mb.value).ok,"move inherited coverage");need(!target.actor_condition_capture_identity()&&!moved.actor_condition_capture_identity(),"move identity retained");
 auto replaced=fixture();auto ab=begin_actor_condition_coverage(replaced);need(ab.ok,"replace begin");replaced=fixture();need(replaced.begin_actor_condition_capture().ok,"replace new capture");need(!finish_actor_condition_coverage(replaced,ab.value).ok&&replaced.finish_actor_condition_capture().ok,"same address replacement stole capture");
 // 非身体字段仍由原writer改变，不列入HP/疲劳覆盖的虚假授权。
 auto unrelated=fixture();auto ub=begin_actor_condition_coverage(unrelated);need(ub.ok,"unrelated begin");ActorMutation v;v.actor_id="p";v.available=false;need(unrelated.apply_actor(v).ok,"unrelated original mutation");auto empty=finish_actor_condition_coverage(unrelated,ub.value);need(empty.ok&&empty.value.empty(),"unrelated actor field counted as condition");
 // 端点覆盖不能发现完全恢复同值且没经原writer的中间直写，明确保留该限制。
 auto neutral=fixture();auto nb=begin_actor_condition_coverage(neutral);need(nb.ok,"neutral begin");neutral.state().actors.find("p")->hp--;neutral.state().actors.find("p")->hp++;auto limited=finish_actor_condition_coverage(neutral,nb.value);need(limited.ok&&limited.value.empty(),"endpoint guard claimed full historical trace");
 std::cout<<"ACTOR_CONDITION_COVERAGE "<<checks<<"/0 TRANSIENT_ENDPOINT_ONLY\n";return 0;
}catch(const std::exception& e){std::cerr<<"ACTOR_CONDITION_COVERAGE "<<checks<<"/1 "<<e.what()<<'\n';return 1;}}

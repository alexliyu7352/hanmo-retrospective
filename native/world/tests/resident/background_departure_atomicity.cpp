// 真正中央NPC出征事务的控制故障：分别在B登记事实前、B完整写入后拒绝，核C+B+共同账本回滚。
#include "resident/kernel/world_codec.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace hanmo::resident;
namespace {
bool inject_after_journey=false,observed_journey_write=false;
// 只读原始oracle，不改磁盘输入；严格解析失败立即终止该窄测试。
BoundaryValue read_source(const std::string& path){std::ifstream file(path);assert(file.good());std::string bytes((std::istreambuf_iterator<char>(file)),{});auto parsed=parse_boundary_json(bytes);assert(parsed.ok);return std::move(parsed.value);}
// 测试保留生产首错，不把预期之外的失败转成通过。
template<class T>T checked(Outcome<T> result){if(!result.ok){std::cerr<<result.error.original.code<<":"<<result.error.original.path<<"\n";std::abort();}return std::move(result.value);}
void require(Status result){if(!result.ok){std::cerr<<result.error.original.code<<":"<<result.error.original.path<<"\n";std::abort();}}
void exact(const BoundaryValue& before,const BoundaryValue& after){auto diff=compare_boundary(before,after);if(!diff.exact()){auto failure=diff.value_difference?diff.value_difference:diff.order_difference;std::cerr<<failure->path<<":"<<failure->reason<<"\n";std::abort();}}
// 共同版本不全在导出世界中；包括尚未存在但本次会触及的action键。
std::vector<ReadVersion> versions(const WorldBundle& w,const StableId& action){std::vector<ReadVersion> out;auto add=[&](ResourceKey::Kind kind,const StableId& id){out.push_back(w.common.read_version({kind,id}));};for(auto& id:w.common.actor_order())add(ResourceKey::Kind::actor,id);for(auto& row:w.common.state().wallets.rows())add(ResourceKey::Kind::wallet,row.first);for(auto& row:w.common.state().inventories.rows())add(ResourceKey::Kind::inventory,row.first);for(auto& id:w.common.action_order())add(ResourceKey::Kind::action,id);for(auto& row:w.common.state().parties.rows())add(ResourceKey::Kind::party,row.first);for(auto& army:w.military.armies)add(ResourceKey::Kind::army,army.id);add(ResourceKey::Kind::action,action);return out;}
// 提案普通失败后，同一个候选下一位办理者的合法动作仍会被真正中央入口采纳。
void next_command(WorldBundle& w,Integer next){CommandPayload move{"move_local",TravelCommandPayload{MoveLocalCommand{"east.camp"}}};bool applied=false;require(try_npc_command(w,"actor.blackwater.commander",move,applied));assert(applied);auto location=std::get_if<SettlementLocation>(&w.common.actor("actor.blackwater.commander")->location);assert(location&&location->place_id=="east.camp");assert(w.common.state().next_instance_id==next);}
// 调用真正try_npc_command，不在测试里重写savepoint或领域备份逻辑。
void reject_and_restore(WorldBundle& w,const CommandPayload& command,bool after_write){const auto next=w.common.state().next_instance_id;const auto action="action:"+std::to_string(next);auto before=checked(export_world_state(w));auto checkpoint=checked(encode_world_checkpoint(w));auto old_versions=versions(w,action);inject_after_journey=after_write;observed_journey_write=false;bool applied=true;require(try_npc_command(w,"actor.blackwater.commander",command,applied));assert(!applied&&!inject_after_journey);assert(observed_journey_write==after_write);assert(w.common.state().next_instance_id==next);assert(!w.common.action(action)&&!w.travel.actions.find(action));exact(before,checked(export_world_state(w)));assert(checkpoint==checked(encode_world_checkpoint(w)));for(auto& version:old_versions)assert(w.common.read_version(version.resource).version==version.version);next_command(w,next);}
}
// 仅测试链接的GNU --wrap接缝；始终先执行原B函数，只在明确武装的那一次成功后注入普通失败。
// 此合成故障不表示合法世界自然会失败，也不改生产库或复制任何旅行/事务规则。
Status real_begin(CommonWrite&,TravelState&,StableId,const TravelQuote&) asm("__real__ZN5hanmo8resident20begin_terrain_travelERNS0_11CommonWriteERNS0_11TravelStateENSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERKNS0_11TravelQuoteE");
Status wrapped_begin(CommonWrite&,TravelState&,StableId,const TravelQuote&) asm("__wrap__ZN5hanmo8resident20begin_terrain_travelERNS0_11CommonWriteERNS0_11TravelStateENSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERKNS0_11TravelQuoteE");
Status wrapped_begin(CommonWrite& common,TravelState& travel,StableId action,const TravelQuote& quote){auto result=real_begin(common,travel,action,quote);if(!result.ok||!inject_after_journey)return result;auto params=travel.actions.find(action);assert(params&&travel.journey_facts.find(params->plan_id));auto core=common.action(action);assert(core&&std::holds_alternative<WildernessLocation>(common.actor(core->actor_id)->location));observed_journey_write=true;inject_after_journey=false;return common_failure("OracleInjectedAfterJourney");}
// 装载已被原GD验证的100人tick4状态；碰撞只施加于独立候选，不作为可恢复种子发布。
int main(int argc,char** argv){assert(argc==2);std::string root=argv[1];auto index=read_source(root+"/oracle-index.json");auto& entry=(*index.find("commands")->as<BoundaryValue::Array>())[7];auto source=read_source(root+"/"+boundary::get_string(*entry.find("after"),"file","after"));auto catalog=checked(prepare_catalog(read_source(root+"/expanded-catalog.json"),source));WorldBundle base(catalog,checked(decode_world_seed(source,*catalog)));require(base.validate());CommandPayload command{"campaign_depart",MilitaryCommand{CampaignDepart{"campaign:197"}}};assert(prepare_command(base,"actor.blackwater.commander",command).ok);
    {WorldBundle collision(base);auto next=collision.common.state().next_instance_id;auto action="action:"+std::to_string(next),journey="journey:"+std::to_string(next+1);assert(!collision.common.state().facts.find(journey));auto payload=collision.military.supply_facts.front();payload.id=journey;collision.military.supply_facts.push_back(payload);require(collision.common.append_fact({journey,"military_supply","military"}));
        {WorldBundle probe(collision);auto quote=checked(prepare_command(probe,"actor.blackwater.commander",command));auto failed=apply_command(probe,"actor.blackwater.commander",*quote->operation);assert(!failed.ok&&failed.error.kind==ErrorKind::rule&&failed.error.original.code=="DuplicateFactId");assert(probe.military.campaigns.front().status=="marching");assert(probe.common.action(action)&&probe.common.actor("actor.blackwater.commander")->action_id==action);assert(probe.common.state().next_instance_id==next+2);assert(!probe.travel.actions.find(action));exact(checked(export_travel_state(collision.travel)),checked(export_travel_state(probe.travel)));}
        reject_and_restore(collision,command,false);
    }
    {WorldBundle post_write(base);reject_and_restore(post_write,command,true);}
    // 同一真实代码在未注入故障时创建军团旅行；证明测试接缝没有用假成功替换B。
    {WorldBundle success(base);bool applied=false;require(try_npc_command(success,"actor.blackwater.commander",command,applied));assert(applied);assert(success.military.campaigns.front().status=="marching");auto action=success.common.actor("actor.blackwater.commander")->action_id;assert(!action.empty()&&success.travel.actions.find(action));require(success.validate());}
    std::cout<<"background_departure_atomicity: PASS (actual try_npc_command; synthetic DuplicateFactId after C/common writes and wrapped ordinary failure after real B writes; full export/checkpoint/ID/read versions restored; next command adopted; unwrapped departure succeeds)\n";
}

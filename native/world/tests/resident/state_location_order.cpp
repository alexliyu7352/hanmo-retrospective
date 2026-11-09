// 从旧Codec排序字段序恢复后，真正的Wilderness.point原位写不能变成字典替换。
#include "resident/storage/common_checkpoint.hpp"
#include <iostream>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace hanmo::resident;
CodecVisitor::Buffer checkpoint(CommonState state){CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);assert(visit_common_checkpoint(writer,state).ok&&writer.finish().ok);return bytes;}
int main(){CommonState seed;ActorRecord walker;walker.id="actor.walker";walker.location=WildernessLocation{"map.test","city.origin",{1,2}};walker.location_order={"kind","map_id","origin","point"};seed.actors.insert(walker.id,walker);CommonStore c(seed,std::make_shared<const CommonCatalog>());auto before=checkpoint(c.state());ActorMutation point;point.actor_id=walker.id;point.location=WildernessLocation{"map.test","city.origin",{8,9}};point.location_mode=ActorMutation::LocationMode::wilderness_point;auto saved=c.begin_savepoint();assert(saved.ok&&c.apply_actor(point).ok);assert(c.state().actors.find(walker.id)->location_order==walker.location_order);auto exported=encode_common_state(c.state());auto* location=exported.find("actors")->find(walker.id)->find("location");assert(capture_field_order(*location).value==walker.location_order);assert(boundary::integer(boundary::array(*location->find("point"),"")[0],"")==8);assert(c.rollback_savepoint(saved.value).ok&&checkpoint(c.state())==before);
 // 原位模式拒绝换类型/换map/换origin，并且其他人物字段也不能提前写入。
 for(int fault=0;fault<5;++fault){auto bad=point;bad.fatigue=90;if(fault==0)bad.location=SettlementLocation{"city.origin","gate"};if(fault==1)std::get<WildernessLocation>(*bad.location).map_id="other";if(fault==2)std::get<WildernessLocation>(*bad.location).origin="other";if(fault==3)bad.location.reset();if(fault==4)bad.location_mode=static_cast<ActorMutation::LocationMode>(99);auto status=c.apply_actor(bad);assert(!status.ok&&status.error.kind==ErrorKind::internal&&status.error.original.code=="InvalidLocationMutation");assert(checkpoint(c.state())==before);}
 // 同一variant也可以是真替换；默认模式必须重置成新writer字面序，不能靠类型相同猜测。
 auto replacement=point;replacement.location_mode=ActorMutation::LocationMode::replace;assert(c.apply_actor(replacement).ok&&c.state().actors.find(walker.id)->location_order.empty());auto output=encode_common_state(c.state());assert(capture_field_order(*output.find("actors")->find(walker.id)->find("location")).value==FieldOrder({"kind","map_id","point","origin"}));std::cout<<"state_location_order: PASS (explicit point patch preserves restored order, replacement resets, bad shape atomic, rollback exact)\n";}

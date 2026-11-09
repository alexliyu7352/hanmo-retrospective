// 只测原Action/载荷的释放前拥有值，不把仍活动或缺原因的快照当合法到达。
#define main private_shop_original_travel_boundary_main
#include "private_shop_travel_expense_source_test.cpp"
#undef main
#include "resident/domains/travel/travel_release_capture.hpp"
#include "resident/storage/common_codec.hpp"
namespace {
// 正常仍在途也可被只读，snapshot不会隐含completed，且后续原载荷改变不写穿旧值。
void original_release_values(){
    Fixture f;auto id=f.begin("carrier","B");auto before=encode_common_state(f.common.state());auto travel_before=export_travel_state(f.travel);require(travel_before.ok,"original Travel encoding");auto read=capture_travel_before_release(f.common,f.travel,id);require(read.ok,"original pre-release snapshot");
    check(read.value.action.id==id&&read.value.action.kind=="travel"&&read.value.action.remaining_ticks==3&&std::holds_alternative<RoadTravelParams>(read.value.original_payload),"actual unfinished action is not mislabeled arrival");
    check(compare_boundary(before,encode_common_state(f.common.state())).exact()&&compare_boundary(travel_before.value,export_travel_state(f.travel).value).exact(),"snapshot does not mutate original banks");
    require(step_travel(f.common,f.travel,f.catalog,f.services,id).ok,"original next paid step");check(std::get<OnRouteLocation>(read.value.actual_location).progress==0&&std::get<OnRouteLocation>(f.common.actor("carrier")->location).progress==100,"old snapshot is owned independent of later actual movement");
    require(f.common.apply_resources({{WalletDelta{"wallet.carrier",-93,true}},{}}).ok&&preview_rescue(f.common,f.travel,f.catalog,f.services,"carrier").ok,"real stranded rescue qualification");require(begin_rescue(f.common,f.travel,"carrier").ok,"actual rescue kind replacement");auto rescue=capture_travel_before_release(f.common,f.travel,id);require(rescue.ok,"actual rescue pre-release snapshot");check(rescue.value.action.kind=="rescue"&&rescue.value.action.remaining_ticks==2&&std::get<RescueParams>(rescue.value.original_payload).origin=="A","rescue request payload does not claim rescue finished");check(read.value.action.kind=="travel"&&std::holds_alternative<RoadTravelParams>(read.value.original_payload),"old travel snapshot survives replacement");
    require(erase_travel_action_payload(f.travel,id).ok,"original generic payload erase");auto missing=capture_travel_before_release(f.common,f.travel,id);check(!missing.ok&&missing.error.original.code=="TravelReleaseOriginalPayloadRequired","erased payload cannot be recreated as arrival source");
}
// 不匹配kind、重复载荷和错误人物占用都不允许只读接缝给出看似完整的原快照。
void invalid_originals(){
    Fixture f;auto id=f.begin("carrier","B");f.travel.rescues.insert(id,{"A"});check(!capture_travel_before_release(f.common,f.travel,id).ok,"dual travel and rescue payload refused");f.travel.rescues.erase(id);
    ActionMutation kind;kind.action=*f.common.action(id);kind.action_kind=StableId{"wait"};require(f.common.apply_action(kind).ok,"original mutation for invalid kind fixture");auto wrong=capture_travel_before_release(f.common,f.travel,id);check(!wrong.ok&&wrong.error.original.code=="TravelReleaseOriginalKindRequired","unrelated action cannot yield travel snapshot");check(!capture_travel_before_release(f.common,f.travel,"missing").ok,"missing actual action refused");
}
}
int main(){try{original_release_values();invalid_originals();}catch(const std::exception& e){check(false,e.what());}std::cout<<"TRAVEL_RELEASE_SNAPSHOT_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}

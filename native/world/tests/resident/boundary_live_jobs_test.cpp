// 确定性检查同worker的任务顺序/撤权/结果保留；无替身游戏规则或真实性能结论。
#include "resident_bridge_live_jobs.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
using V=BoundaryValue;
// 测试身份与回执只模拟线程边界结果，不调用或宣称世界推进。
LiveOperation op(LiveOperationKind kind,const WorldIdentity& source,const char* run="run:a") {
    V::Dict payload;if(kind==LiveOperationKind::begin_run||kind==LiveOperationKind::grant||kind==LiveOperationKind::cancel_run)payload.emplace_back("run_id",V{std::string(run)});
    if(kind==LiveOperationKind::grant)payload.emplace_back("grant_half_days",V{Integer{1}});
    return {kind,source,{std::move(payload)}};
}
Outcome<LiveOperationResult> finished(const ResidentLiveTicket& ticket,const WorldIdentity& identity,const char* stop="awaiting_permit") {return {true,{ticket.operation.source,identity,{V::Dict{{"ok",{true}},{"stop",{std::string(stop)}}}}},{}};}
int main() {
    WorldIdentity id;id.world_id[0]=7;id.execution_epoch=3;ResidentLiveJobs jobs;jobs.bind(id);
    auto begin=jobs.enqueue(op(LiveOperationKind::begin_run,id),1);assert(begin.ok);
    assert(jobs.set_reading_paused({id.world_id,id.execution_epoch,"run:a"},true).ok);
    auto cancel=jobs.enqueue(op(LiveOperationKind::cancel_run,id),2);assert(cancel.ok);
    auto duplicate=jobs.enqueue(op(LiveOperationKind::cancel_run,id),3);assert(duplicate.ok&&duplicate.value==2);
    auto first=jobs.take();assert(first&&first->operation_id==1);assert(!jobs.take());jobs.complete(1,finished(*first,id),id);
    auto second=jobs.take();assert(second&&second->operation_id==2);assert(jobs.enqueue(op(LiveOperationKind::cancel_run,id),77).value==2);jobs.complete(2,finished(*second,id,"cancelled"),id);
    assert(jobs.poll(id,2).value.ready);assert(!jobs.enqueue(op(LiveOperationKind::query,id),3).ok);
    assert(jobs.poll(id,1).value.ready);assert(jobs.poll(id,1).value.result.find("stop"));
    assert(jobs.enqueue(op(LiveOperationKind::grant,id),3).ok);auto repeated=jobs.take();assert(repeated&&repeated->operation_id==3);jobs.complete(3,finished(*repeated,id,"cancelled"),id);assert(jobs.poll(id,3).value.ready); // 终止后同run重复许可仍交core返回缓存，不能被transport改成UnknownRun。
    assert(jobs.enqueue(op(LiveOperationKind::begin_run,id,"run:b"),4).ok);assert(!jobs.poll(id,1).ok);
    auto b=jobs.take();assert(b&&b->operation_id==4);jobs.complete(4,finished(*b,id),id);assert(jobs.poll(id,4).ok);
    auto grant=jobs.enqueue(op(LiveOperationKind::grant,id,"run:b"),5);assert(grant.ok);
    assert(jobs.set_reading_paused({id.world_id,id.execution_epoch,"run:b"},true).ok);assert(!jobs.take());
    auto revoked=jobs.poll(id,5);assert(revoked.ok&&revoked.value.ready&&*revoked.value.result.find("code")->as<StableId>()=="OperationRevoked");
    assert(jobs.set_reading_paused({id.world_id,id.execution_epoch,"run:b"},false).ok&&!jobs.has_work());
    assert(jobs.enqueue(op(LiveOperationKind::grant,id,"run:b"),6).ok);auto active=jobs.take();assert(active&&active->operation_id==6);
    auto wrong=id;++wrong.execution_epoch;assert(!jobs.enqueue(op(LiveOperationKind::cancel_run,wrong,"run:b"),7).ok);
    assert(jobs.enqueue(op(LiveOperationKind::cancel_run,id,"run:b"),7).ok);assert(!jobs.enqueue(op(LiveOperationKind::query,id),8).ok);
    auto next=id;++next.tick;++next.revision;jobs.complete(6,finished(*active,next),next);assert(jobs.poll(id,6).value.current.tick==1);
    auto priority=jobs.take();assert(priority&&priority->operation_id==7);jobs.complete(7,finished(*priority,next,"cancelled"),next);
    assert(jobs.poll(id,7).value.ready);assert(jobs.enqueue(op(LiveOperationKind::query,next),8).ok);
    auto query=jobs.take();jobs.complete(8,finished(*query,next),next);auto value=jobs.poll(next,8);*value.value.result.find("ok")=V{false};assert(*jobs.poll(next,8).value.result.find("ok")->as<bool>());
    auto stale=next;++stale.execution_epoch;assert(!jobs.poll(stale,8).ok);
    assert(jobs.enqueue(op(LiveOperationKind::command,id),9).ok); // stale revision不在transport抢先拒绝缓存重放。
    ResidentLiveJobs restores;restores.bind(id);assert(restores.enqueue(op(LiveOperationKind::begin_run,id),10).ok);auto rb=restores.take();restores.complete(10,finished(*rb,id),id);assert(restores.poll(id,10).ok);
    assert(restores.enqueue(op(LiveOperationKind::restore,id),11).ok);auto failed_restore=restores.take();restores.complete(11,{false,{},{{"CorruptCheckpoint",{}},ErrorKind::rule,{},{},{},{}}},id);assert(restores.poll(id,11).value.current.execution_epoch==id.execution_epoch);assert(restores.set_reading_paused({id.world_id,id.execution_epoch,"run:a"},true).ok);
    assert(restores.enqueue(op(LiveOperationKind::restore,id),12).ok);auto good_restore=restores.take();auto replacement=id;++replacement.execution_epoch;restores.complete(12,finished(*good_restore,replacement),replacement);assert(restores.poll(id,12).value.current.execution_epoch==replacement.execution_epoch);assert(!restores.poll(id,11).ok);assert(!restores.set_reading_paused({id.world_id,id.execution_epoch,"run:a"},false).ok);
    assert(restores.enqueue(op(LiveOperationKind::query,replacement),13).ok);assert(!restores.poll(id,12).ok);
    ResidentLiveJobs binary;binary.bind(id);auto input=op(LiveOperationKind::restore,id);input.checkpoint_bytes=Bytes{0,255,128,7};input.previous_checkpoint_bytes=Bytes{};assert(binary.enqueue(std::move(input),20).ok);auto packet=binary.take();assert(packet&&packet->operation.checkpoint_bytes==Bytes({0,255,128,7})&&packet->operation.previous_checkpoint_bytes&&packet->operation.previous_checkpoint_bytes->empty());
    auto output=finished(*packet,id);output.value.checkpoint_bytes=Bytes{0,255,128,9};binary.complete(20,std::move(output),id);auto first_bytes=binary.poll(id,20);assert(first_bytes.value.checkpoint_bytes&&*first_bytes.value.checkpoint_bytes==Bytes({0,255,128,9}));auto again_bytes=binary.poll(id,20);assert(again_bytes.value.checkpoint_bytes&&*again_bytes.value.checkpoint_bytes==*first_bytes.value.checkpoint_bytes);
    std::cout<<"live job scheduling checks passed; no world/rule execution in this test\n";
}

// 有界调度状态的实现；所有实际规则调用仍由Bridge唯一worker执行。
#include "resident_bridge_live_jobs.hpp"
#include <algorithm>

namespace hanmo::resident {
namespace {
using V=BoundaryValue;
// 这里只比较代次和小控制字段，不抢先执行actor/replay/revision或领域守卫。
bool same_world(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch;}
// 控制字段只取字符串；错误形状在接纳前明确拒绝。
StableId run_id(const LiveOperation& op){const auto* value=op.payload.find("run_id");return value&&value->as<StableId>()?*value->as<StableId>():StableId{};}
// 同时绑定世界、代次和run ID，不拿revision阻止取消。
bool matching_run(const RunRef& run,const LiveOperation& op){return run.world_id==op.source.world_id&&run.execution_epoch==op.source.execution_epoch&&run.run_id==run_id(op);}
// 调度拒绝不生成规则成功值，也不触碰core。
template<class T> Outcome<T> refused(const char* code){return {false,{},{{code,{}},ErrorKind::rule,{},{},{},{}}};}
// core失败的原有定位全部保留；transport不把失败包装成成功的世界结果。
V failure_value(const RuleError& error){return {V::Dict{{"ok",{false}},{"code",{error.original.code}},{"path",{error.original.path}},{"kind",{std::string(error.kind==ErrorKind::rule?"rule":error.kind==ErrorKind::unsupported_capability?"unsupported_capability":"internal")}},{"phase",{error.phase}},{"domain",{error.domain}},{"command",{error.command}},{"source_id",{error.source_id}}}};}
// 仅识别core明确成功，用于释放transport授权元数据。
bool succeeded(const V& result){auto p=result.find("ok");return p&&p->as<bool>()&&*p->as<bool>();}
// 读取有限run stop，不推算世界结束条件。
StableId stopped(const V& result){auto p=result.find("stop");return p&&p->as<StableId>()?*p->as<StableId>():StableId{};}
}
// bind只在安装/明确替换入口调用，丢弃旧transport授权而不触碰core。
void ResidentLiveJobs::bind(const WorldIdentity& id){clear();identity_=id;}
// 最多两条记录线性定位，未知票据绝不借用另一条结果。
ResidentLiveJobs::Record* ResidentLiveJobs::find(Integer id){for(auto& record:records_)if(record.ticket.operation_id==id)return &record;return nullptr;}
// 未消费结果仍占槽，慢UI不能造成无界完成队列。
bool ResidentLiveJobs::busy()const{return std::any_of(records_.begin(),records_.end(),[](const auto& row){return !row.consumed;});}
// worker只以明确待办唤醒，没有自动续发许可。
bool ResidentLiveJobs::has_work()const{return normal_.has_value()||cancel_.has_value();}
// 释放仅transport状态；销毁线程和世界仍由外层Bridge负责。
void ResidentLiveJobs::clear(){records_.clear();normal_.reset();cancel_.reset();running_.reset();run_.reset();reading_paused_=false;run_detached_=false;}
// 普通任务仅一条；取消槽允许在当前完整事务未结束时及时撤掉后续许可。
Outcome<Integer> ResidentLiveJobs::enqueue(LiveOperation operation,Integer id){
    if(!same_world(identity_,operation.source))return refused<Integer>("ResidentStaleReference");
    if(id<=0)return refused<Integer>("ResidentOperationLimit");
    const auto kind=operation.kind;const auto requested_run=run_id(operation);
    if((kind==LiveOperationKind::begin_run||kind==LiveOperationKind::grant||kind==LiveOperationKind::cancel_run)&&requested_run.empty())return refused<Integer>("ResidentInvalidRunRef");
    if(kind==LiveOperationKind::cancel_run){
        for(const auto& row:records_)if(row.ticket.operation.kind==kind&&row.run_id==requested_run)return {true,row.ticket.operation_id,{}};
        if(!run_||!matching_run(*run_,operation))return refused<Integer>("ResidentInvalidRunRef");
        if(cancel_||records_.size()>=2)return refused<Integer>("ResidentBridgeBusy");
        revoke_pending_grant();reading_paused_=true;
        records_.push_back({{id,std::move(operation)},false,false,identity_,{}});records_.back().run_id=requested_run;cancel_=id;return {true,id,{}};
    }
    if(busy())return refused<Integer>("ResidentBridgeBusy");
    if(kind==LiveOperationKind::begin_run&&run_&&!run_detached_)return refused<Integer>("ResidentRunActive");
    if(kind==LiveOperationKind::grant){
        if(!run_||!matching_run(*run_,operation))return refused<Integer>("ResidentInvalidRunRef");
        const auto* grant=operation.payload.find("grant_half_days");if(!grant||!grant->as<Integer>()||*grant->as<Integer>()!=1)return refused<Integer>("InvalidRunPermit");
        if(reading_paused_)return refused<Integer>("ResidentReadingPaused");
    }
    // 只有新普通任务真正接纳时才回收已消费结果，旧ticket此前始终可重复读取。
    records_.clear();records_.push_back({{id,std::move(operation)},false,false,identity_,{}});normal_=id;records_.back().run_id=requested_run;
    if(kind==LiveOperationKind::begin_run){run_=RunRef{identity_.world_id,identity_.execution_epoch,requested_run};reading_paused_=false;run_detached_=false;}
    return {true,id,{}};
}
// begin预留尚未落core时必须先begin后cancel；其余取消优先于未开始任务。
std::optional<ResidentLiveTicket> ResidentLiveJobs::take(){
    if(running_)return {};
    std::optional<Integer> chosen;
    if(cancel_){const auto* normal=normal_?find(*normal_):nullptr;if(normal&&normal->ticket.operation.kind==LiveOperationKind::begin_run){chosen=normal_;normal_.reset();}else{chosen=cancel_;cancel_.reset();}}
    else if(normal_){chosen=normal_;normal_.reset();}
    if(!chosen)return {};
    auto* record=find(*chosen);if(!record)return {};
    running_=chosen;ResidentLiveTicket picked{*chosen,std::move(record->ticket.operation)};
    // 普通回执只需tag/source/run ID；冷输入bytes移动给worker，不在缓存多留一份大blob。
    record->ticket.operation={picked.operation.kind,picked.operation.source,{}};return picked;
}
// 一个完成仅替换自己的有限结果；当前普通和cancel两张已接纳票据互不覆盖。
void ResidentLiveJobs::complete(Integer id,Outcome<LiveOperationResult> outcome,const WorldIdentity& actual){
    if(!running_||*running_!=id)return;
    running_.reset();auto* record=find(id);if(!record)return;
    const auto kind=record->ticket.operation.kind;
    if(!same_world(identity_,actual)){
        Record current=std::move(*record);records_.clear();records_.push_back(std::move(current));record=&records_.front();normal_.reset();cancel_.reset();run_.reset();reading_paused_=false;run_detached_=false;
    }
    identity_=actual;record->current=actual;record->checkpoint_failure=std::move(outcome.value.checkpoint_failure);record->checkpoint_capture=std::move(outcome.value.checkpoint_capture);record->result=outcome.ok?std::move(outcome.value.result):record->checkpoint_failure?V{}:failure_value(outcome.error);record->checkpoint_bytes.reset();record->continuation_bytes.reset();
    if(outcome.ok&&outcome.value.checkpoint_bytes)record->checkpoint_bytes=std::make_shared<const Bytes>(std::move(*outcome.value.checkpoint_bytes));
    if(outcome.ok&&outcome.value.continuation_bytes)record->continuation_bytes=std::make_shared<const Bytes>(std::move(*outcome.value.continuation_bytes));
    record->ready=true;
    if(kind==LiveOperationKind::begin_run&&!succeeded(record->result)){run_.reset();reading_paused_=false;run_detached_=false;}
    if((kind==LiveOperationKind::cancel_run&&succeeded(record->result))||((kind==LiveOperationKind::grant||kind==LiveOperationKind::begin_run)&&succeeded(record->result)&&stopped(record->result)!="awaiting_permit")){run_detached_=true;reading_paused_=false;}
}
// ready的首次poll消费槽位但仍返回深拷贝；后续普通任务接纳前旧结果可重复读取。
Outcome<ResidentLivePoll> ResidentLiveJobs::poll(const WorldIdentity& source,Integer id){
    auto* record=find(id);if(!record||!same_world(source,record->ticket.operation.source))return refused<ResidentLivePoll>("ResidentStaleReference");
    if(!same_world(source,identity_)&&record->ticket.operation.kind!=LiveOperationKind::restore)return refused<ResidentLivePoll>("ResidentStaleReference");
    if(!record->ready)return {true,{false,record->ticket.operation.kind,record->ticket.operation.source,identity_,{}},{}};
    record->consumed=true;ResidentLivePoll result{true,record->ticket.operation.kind,record->ticket.operation.source,record->current,record->result,record->checkpoint_bytes,record->continuation_bytes,record->checkpoint_failure,record->checkpoint_capture};record->checkpoint_capture.reset();return {true,std::move(result),{}};
}
// 只撤尚未被take认领的grant；已执行的完整事务由core自己提交或回滚。
void ResidentLiveJobs::revoke_pending_grant(){
    if(!normal_)return;
    auto* record=find(*normal_);if(!record||record->ticket.operation.kind!=LiveOperationKind::grant)return;
    record->ready=true;record->current=identity_;record->result={V::Dict{{"ok",{false}},{"code",{std::string("OperationRevoked")}},{"stop",{std::string("revoked_before_start")}}}};normal_.reset();
}
// 已接纳begin的预留RunRef同样可撤权；解除只改变门的布尔值。
Status ResidentLiveJobs::set_reading_paused(const RunRef& reference,bool paused){
    if(!run_||reference.world_id!=run_->world_id||reference.execution_epoch!=run_->execution_epoch||reference.run_id!=run_->run_id)return refused<Done>("ResidentInvalidRunRef");
    reading_paused_=paused;if(paused)revoke_pending_grant();return {true,{},{}};
}
} // 解除reading门不会添加任何待执行操作。

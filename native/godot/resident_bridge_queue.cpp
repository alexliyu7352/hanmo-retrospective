// 单writer队列不承担规则推进；每个job原样交给唯一ResidentWorld执行端口。
#include "resident_bridge_queue.hpp"
#include <stdexcept>
#include <algorithm>

namespace hanmo::resident {
namespace {
// 错误码只表示Bridge许可边界，规则错误由core原样回传。
Status fail(const std::string& code) {return {false,{},{{code,"resident_bridge"},ErrorKind::rule,{},{},{},{}}};}
// 运行引用全部字段共同判等，run_id不能绕过world/epoch检查。
bool same(const RunRef& a,const RunRef& b) {return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.run_id==b.run_id;}
}
ResidentBridgeQueue::ResidentBridgeQueue(std::unique_ptr<ResidentRunExecutor> executor):executor_(std::move(executor)) {
    if(!executor_) throw std::invalid_argument("ResidentRunExecutor required");
    worker_=std::thread([this]{work();});
}
ResidentBridgeQueue::~ResidentBridgeQueue() {close();}
Status ResidentBridgeQueue::check_ref(const RunRef& ref) const {
    if(closing_) return fail("ResidentBridgeClosed");
    if(!active_||!same(*active_,ref)) return fail("StaleRunReference");
    return {true,{},{}};
}
Status ResidentBridgeQueue::begin(const BeginRun& request) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(closing_) return fail("ResidentBridgeClosed");
    if(active_) return fail("ResidentBridgeRunAlreadyBound");
    active_=RunRef{request.expected.world_id,request.expected.execution_epoch,request.run_id};
    progress_={}; progress_.cached.identity=request.expected; progress_.cached.run_id=request.run_id; progress_.pending=true;
    Job job; job.kind=Kind::begin; job.begin=request; job.reference=*active_; jobs_.push_back(std::move(job)); wake_.notify_one();
    return {true,{},{}};
}
Status ResidentBridgeQueue::grant(const ResumeRun& request) {
    std::lock_guard<std::mutex> lock(mutex_);
    const RunRef ref{request.expected.world_id,request.expected.execution_epoch,request.run_id}; const auto checked=check_ref(ref); if(!checked.ok) return checked;
    if(request.grant_half_days!=1) return fail("ResidentBridgeOneStepRequired");
    if(progress_.reading_paused) return fail("ResidentBridgeReadingPaused");
    if(progress_.cached.stop!=RunStop::awaiting_permit&&request.permit_seq!=progress_.cached.permit_seq) return fail("ResidentBridgeRunStopped");
    if(progress_.running||!jobs_.empty()) return fail("ResidentBridgeBusy");
    Job job; job.kind=Kind::resume; job.resume=request; job.reference=ref; jobs_.push_back(std::move(job)); progress_.pending=true; wake_.notify_one();
    return {true,{},{}};
}
Outcome<ResidentBridgeProgress> ResidentBridgeQueue::poll(const RunRef& ref) const {
    std::lock_guard<std::mutex> lock(mutex_); const auto checked=check_ref(ref);
    if(!checked.ok) return {false,{},checked.error};
    return {true,progress_,{}};
}
Status ResidentBridgeQueue::request_cancel(const RunRef& ref,bool reading) {
    std::lock_guard<std::mutex> lock(mutex_); const auto checked=check_ref(ref); if(!checked.ok) return checked;
    if(reading) progress_.reading_paused=true;
    // begin尚未运行时保留创建停驻run；只撤销许可，不把不存在的run交给cancel。
    jobs_.erase(std::remove_if(jobs_.begin(),jobs_.end(),[](const Job& j){return j.kind!=Kind::begin;}),jobs_.end());
    if(!reading) { Job job; job.kind=Kind::cancel; job.reference=ref; jobs_.push_back(std::move(job)); }
    progress_.pending=!jobs_.empty(); wake_.notify_one();
    return {true,{},{}};
}
Status ResidentBridgeQueue::pause_for_reading(const RunRef& ref) {return request_cancel(ref,true);}
Status ResidentBridgeQueue::resume_after_reading(const RunRef& ref) {
    std::lock_guard<std::mutex> lock(mutex_); const auto checked=check_ref(ref); if(!checked.ok) return checked;
    progress_.reading_paused=false; return {true,{},{}};
}
Status ResidentBridgeQueue::cancel(const RunRef& ref) {return request_cancel(ref,false);}
void ResidentBridgeQueue::close() {
    { std::lock_guard<std::mutex> lock(mutex_); if(closing_) return; closing_=true; jobs_.clear(); wake_.notify_one(); }
    if(worker_.joinable()) worker_.join();
}
void ResidentBridgeQueue::work() {
    while(true) {
        Job job;
        { std::unique_lock<std::mutex> lock(mutex_); wake_.wait(lock,[this]{return closing_||!jobs_.empty();}); if(closing_) return;
          job=std::move(jobs_.front()); jobs_.pop_front(); progress_.pending=!jobs_.empty(); progress_.running=true; }
        Outcome<RunProgress> result;
        try { if(job.kind==Kind::begin) result=executor_->begin(job.begin); else if(job.kind==Kind::resume) result=executor_->resume(job.resume); else result=executor_->cancel(job.reference); }
        catch(...) { result.error={ {"ResidentBridgeInternalError","resident_bridge"},ErrorKind::internal,{},{},{},{}}; }
        { std::lock_guard<std::mutex> lock(mutex_); progress_.running=false;
          if(result.ok) progress_.cached=std::move(result.value); else {progress_.cached.stop=RunStop::failed; progress_.cached.error=std::move(result.error);}
          progress_.pending=!jobs_.empty(); }
    }
}
} // 关闭只取消后续许可；已开始事务由core原子完成或完整失败。

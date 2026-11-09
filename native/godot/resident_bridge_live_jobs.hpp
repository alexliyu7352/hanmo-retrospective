// 现有Bridge worker的有界调度值对象；不拥有world、线程、Godot对象或任何规则实现。
#pragma once
#include "resident/kernel/boundary_dispatch.hpp"

namespace hanmo::resident {
struct ResidentLiveTicket { Integer operation_id=0; LiveOperation operation; };
struct ResidentLivePoll { bool ready=false; LiveOperationKind tag=LiveOperationKind::query; WorldIdentity source,current; BoundaryValue result; std::shared_ptr<const Bytes> checkpoint_bytes{},continuation_bytes{}; CheckpointJobFailureLease checkpoint_failure; std::shared_ptr<const WorldCheckpointCapture> checkpoint_capture; };
// 外层Bridge同一小mutex保护全部调用；最多普通任务与优先取消各一条未消费结果。
class ResidentLiveJobs {
public:
    void bind(const WorldIdentity& identity);
    Outcome<Integer> enqueue(LiveOperation operation,Integer operation_id);
    std::optional<ResidentLiveTicket> take();
    void complete(Integer operation_id,Outcome<LiveOperationResult> result,const WorldIdentity& actual);
    Outcome<ResidentLivePoll> poll(const WorldIdentity& source,Integer operation_id);
    Status set_reading_paused(const RunRef& reference,bool paused);
    bool busy() const;
    bool has_work() const;
    bool has_run() const { return run_.has_value()&&!run_detached_; }
    void clear();
private:
    struct Record { ResidentLiveTicket ticket; bool ready=false,consumed=false; WorldIdentity current; BoundaryValue result; std::shared_ptr<const Bytes> checkpoint_bytes{},continuation_bytes{}; StableId run_id{}; CheckpointJobFailureLease checkpoint_failure; std::shared_ptr<const WorldCheckpointCapture> checkpoint_capture; };
    WorldIdentity identity_;
    std::optional<RunRef> run_;
    bool reading_paused_=false,run_detached_=false;
    std::optional<Integer> normal_,cancel_,running_;
    std::vector<Record> records_;
    Record* find(Integer operation_id);
    void revoke_pending_grant();
};
} // 调度决定何时调用唯一core，绝不自行推进、完成或回滚世界。

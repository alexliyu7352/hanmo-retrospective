// 薄Bridge的单worker许可队列；主线程只读深拥有进度，不接触Node或候选状态。
#pragma once
#include "hanmo/resident_world_api.hpp"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace hanmo::resident {
// 同一core的窄执行端口；测试替身只能检验协议，不能证明规则或性能。
class ResidentRunExecutor {
public:
    virtual ~ResidentRunExecutor()=default;
    virtual Outcome<RunProgress> begin(const BeginRun&)=0;
    virtual Outcome<RunProgress> resume(const ResumeRun&)=0;
    virtual Outcome<RunProgress> cancel(const RunRef&)=0;
};
// cached是最后已完成事务的进度，running期间不伪称取消已完成。
struct ResidentBridgeProgress { RunProgress cached; bool running=false,pending=false,reading_paused=false; };
class ResidentBridgeQueue {
public:
    explicit ResidentBridgeQueue(std::unique_ptr<ResidentRunExecutor>);
    ~ResidentBridgeQueue();
    ResidentBridgeQueue(const ResidentBridgeQueue&)=delete;
    ResidentBridgeQueue& operator=(const ResidentBridgeQueue&)=delete;
    Status begin(const BeginRun&);
    // 每个Main timer最多授予一半日；不接受隐式批量或旧epoch回调。
    Status grant(const ResumeRun&);
    Outcome<ResidentBridgeProgress> poll(const RunRef&) const;
    // 阅读/失焦立刻撤销尚未开始的许可；恢复阅读只开闸，仍须新的timer显式grant。
    Status pause_for_reading(const RunRef&);
    Status resume_after_reading(const RunRef&);
    Status cancel(const RunRef&);
    // 销毁/显式关闭可join；正常poll/grant/pause从不等待长事务。
    void close();
private:
    enum class Kind { begin,resume,cancel };
    struct Job { Kind kind; BeginRun begin; ResumeRun resume; RunRef reference; };
    std::unique_ptr<ResidentRunExecutor> executor_;
    mutable std::mutex mutex_; std::condition_variable wake_;
    std::deque<Job> jobs_; std::thread worker_;
    std::optional<RunRef> active_; ResidentBridgeProgress progress_;
    bool closing_=false;
    Status check_ref(const RunRef&) const;
    Status request_cancel(const RunRef&,bool reading);
    void work();
};
} // executor只由worker调用；共享锁从不覆盖完整事务执行。

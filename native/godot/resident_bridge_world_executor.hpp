// 产品Bridge执行器只转发同一ResidentWorld；初始化成功后独占所有权移交worker。
#pragma once
#include "resident_bridge_queue.hpp"
namespace hanmo::resident {
class ResidentWorldRunExecutor final : public ResidentRunExecutor {
public:
    explicit ResidentWorldRunExecutor(std::unique_ptr<ResidentWorld> world):world_(std::move(world)) {}
    Outcome<RunProgress> begin(const BeginRun& request) override {return world_->begin_run(request);}
    Outcome<RunProgress> resume(const ResumeRun& request) override {return world_->resume_run(request);}
    Outcome<RunProgress> cancel(const RunRef& request) override {return world_->cancel_run(request);}
private:
    std::unique_ptr<ResidentWorld> world_;
};
} // 同一core运行，不在Bridge内复制advance或领域规则。

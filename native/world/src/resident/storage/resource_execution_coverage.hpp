// 同次原资源捕获的前后覆盖检查；不证明业务授权、永久历史或零净直接写从未发生。
#pragma once
#include "resource_execution_draft.hpp"
namespace hanmo::resident {
class CommonStore;
// 私有持有真实开始前值；副本共享一次消费状态，不能跨候选或重复取走来源。
class ResourceExecutionCoverageBoundary {
public:
    ResourceExecutionCoverageBoundary()=default;
private:
    struct State;
    std::shared_ptr<State> state_;
    friend Outcome<ResourceExecutionCoverageBoundary> begin_resource_execution_coverage(CommonStore&);
    friend Outcome<std::vector<ResourceExecutionDraft>> finish_resource_execution_coverage(
        CommonStore&,ResourceExecutionCoverageBoundary&);
};
// 显式开启原capture后保存原全部钱包/库存值；仅新调用者使用，不改变旧资源入口。
Outcome<ResourceExecutionCoverageBoundary> begin_resource_execution_coverage(CommonStore&);
// 折叠实际每个原语的前后值后核末态；失败不交付draft，不替调用方发布或回滚World。
// 调用方须在原未发布候选内使用，失败丢弃整个候选；本口不会“修正”可疑资源。
Outcome<std::vector<ResourceExecutionDraft>> finish_resource_execution_coverage(
    CommonStore&,ResourceExecutionCoverageBoundary&);
}

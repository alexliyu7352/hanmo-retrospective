// 原Director调用栈持有特殊责任政治读；任何World提交或复制后不得继续借用本对象。
#pragma once
#include "task_responsibility_director.hpp"
#include "resident/kernel/diplomacy_integration.hpp"
namespace hanmo::resident {
struct WorldBundle;
class TaskResponsibilityDirectorWorldAccess final {
public:
    static Outcome<std::unique_ptr<TaskResponsibilityDirectorWorldAccess>> create(const WorldBundle&,const OrganizationRead&);
    const TaskResponsibilityDirectorContext& context()const{return context_;}
    TaskResponsibilityDirectorWorldAccess(const TaskResponsibilityDirectorWorldAccess&)=delete;
    TaskResponsibilityDirectorWorldAccess& operator=(const TaskResponsibilityDirectorWorldAccess&)=delete;
private:
    TaskResponsibilityDirectorWorldAccess(const WorldBundle&,const OrganizationRead&);
    const WorldBundle& world_;
    InteractionAccess interaction_;
    std::optional<DiplomaticPresentRead> present_;
    std::optional<DiplomaticMissionAccess> missions_;
    std::optional<DiplomaticReportAccess> reports_;
    TaskResponsibilityDirectorContext context_;
};
}

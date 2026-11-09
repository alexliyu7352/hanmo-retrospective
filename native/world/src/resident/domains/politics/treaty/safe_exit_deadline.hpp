// 原SafeExit绝对期限的热预测；不新增到期Fact或第二时间真值。
#pragma once
#include "safe_exit_window.hpp"
namespace hanmo::resident {
enum class SafeExitDeadlineKind {install_termination_window,expire_exit_window};
struct SafeExitDeadline {
 SourceRef binding_source;
 SafeExitDeadlineKind kind=SafeExitDeadlineKind::install_termination_window;
 WorldMoment at;
 SourceRef original_termination_basis;
 std::optional<SourceRef> first_actual_exit_entry;
};
// 原源定义的未来界或当前尚待采用界。过界未采用是坏源，不把它顺延到现在。
Outcome<std::vector<SafeExitDeadline>> read_safe_exit_deadlines(const SafeExitEntryContext&);
// 真实writer后fresh reader核已采用后态；不要求已消耗的预测仍存在。
Status validate_safe_exit_deadline_applied(const SafeExitEntryContext&,const SourceRef& binding_source,
 SafeExitDeadlineKind,const WorldMoment& adopted_at);
// 只在唯一调度器选中的真实M调用；Status成功可无业务成果。原binding/line为唯一写槽.
// 尚未接通用精确图；不能把返回binding源的入境时点冒充到期事实时点。
Status apply_safe_exit_deadline_in_candidate(CommonStore&,SafeExitState&,
 const SafeExitEntryContext&,const SafeExitDeadline&);
}

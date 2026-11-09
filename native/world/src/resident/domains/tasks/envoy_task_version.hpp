// 使者返报版本是明确模块边界；外交通行等相邻能力不能隐式启用新的任务责任或历史命令。
#pragma once
#include "envoy_task_state.hpp"
namespace hanmo::resident {
// registered_version只能由已验证的模块marker传入，0表示缺域；本函数不读取作者目录、不改变Common字节。
Status validate_envoy_task_version_boundary(const CommonStore&,Integer registered_version,const EnvoyTaskState* payload);
}

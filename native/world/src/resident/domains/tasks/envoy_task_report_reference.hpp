// 使者v2最终履约引用政治唯一报告；不从结束的Action或外交同意推导返报成功。
#pragma once
#include "envoy_task_contract.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_report_contract.hpp"
namespace hanmo::resident {
// 同候选完成与冷验证复用此逐源绑定；政治读取口拥有报告历史权限与阶段时效，完整Task校验仍负责状态/时钟。
Status validate_envoy_task_report_reference(const CommonStore&,Integer payload_version,const EnvoyTaskSource&,const DiplomaticEnvoyReportRead*);
}

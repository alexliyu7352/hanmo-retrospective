// B1.1 初始职责只读：有限具名目录地址，不制造发生事实或当前履责权限。
#pragma once
#include "organization_authored_origin.hpp"

namespace hanmo::resident {
// 仅接受 organization/authored_duty/<原 duty 行 ID>，并覆核同组织原 duty
// active 集合均有原 policy 引用，所有声明均反查真行；显式 inactive 只证明身份。
// 返回值只在同一不可变目录寿命内借用。
// 传递 obligation_ids 通过 S5 原读口核实际安装行和原双方，不重复建立责任。
// 成功仅证明原作者责任定义身份，不证明 role 今日持有人、工资已付、commitment
// 成果、其他种类责任或完整初始可安装；这些仍由各原 owner 与完整 validator
// 核验。
Outcome<const OrgDuty *>
read_organization_authored_duty(const OrganizationAuthoredOriginContext &,
                                const OrganizationRecord &,
                                const LifecycleSeedPolicy &, const SourceRef &);
} // namespace hanmo::resident

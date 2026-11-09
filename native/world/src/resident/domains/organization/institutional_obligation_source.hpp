// 原机构承诺的应计来源与当前钱包支出权分开；真实欠期不因暂不能付款而消失。
#pragma once
#include "organization_model.hpp"
namespace hanmo::resident {
// 仅原目录institutional_issuer；具名成员工资沿S5唯一接受绑定单独读取。
bool institutional_issuer_obligation_accrual_allowed(const CommonStore&,const OrganizationState&,
    const OrganizationCatalog&,const LocalEconomyState&,const StableId& issuer_id,
    const StableId& obligation_id,const StableId& principal_organization_id,
    const StableId& source_container_id,Integer amount,Integer due_tick);
}

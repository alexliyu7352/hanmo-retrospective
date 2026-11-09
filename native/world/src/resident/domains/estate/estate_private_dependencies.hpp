// 私产交割只读庄园真实绑定和未完工条款；不授权产业继承，也不代替工资或Task合同清算。
#pragma once
#include "estate_model.hpp"
#include "../local_economy/private_account_dependencies.hpp"
namespace hanmo::resident {
// 完整核原Estate/行动事实后返回选中容器的当前阻断；已经支付建造成本不等于订单完成。
Outcome<EconomicPrivateAccountDependencies> read_estate_private_account_dependencies(
    const CommonStore&,const EstateState&,const EstateCatalog&,const CommonCatalog&,
    const EconomicPrivateAccountSelection&,const EstateAccess* access=nullptr);
}

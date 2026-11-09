// 收费冷读逐原开工/完工事实重建受益组织，不从今天Owner或钱包余额推定过去经营权。
#pragma once
#include "naval_repair_charges.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
// 在原军事占领史和Common头已验证后消费；本口仍精确核各引用头、顺序及唯一工单。
Status validate_naval_repair_charge_history(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&);
}

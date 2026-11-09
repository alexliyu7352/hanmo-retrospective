// 收费工法的原子接头：只给原维修Action追加真实公库预约，不新增独立付款命令。
#pragma once
#include "naval_repair_charges_model.hpp"
#include "naval_battle_state.hpp"
#include "resident/model/common_operations.hpp"
namespace hanmo::resident {
// 只在Naval4完整profile下验证；旧免费实物工法仍由原Naval3入口处理。
Status validate_naval_repair_charge_policy(const NavalRepairChargePolicy&,const NavalCatalog&,const MilitaryCatalog&,const CommonCatalog*);
// 泊地须具名服务；浮泊自修不消费公营服务。当前失权或受益组织换主均拒绝新工单。
Outcome<std::optional<NavalRepairChargeTerms>> preview_naval_repair_charge(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalCatalog&,const NavalRepairChargePolicy&,const NavalRepairTerms&);
// 开工预约与物料预约并入同一原recipe；保留原Action作为唯一ReservationRef。
Status append_naval_repair_charge_reservation(const CommonRead&,const NavalRepairChargeTerms&,const StableId& action_id,ResourceRecipe&);
// 完工只在冻结权源仍成立时向原收款库等额支付；失败出口仅由ReleaseActionResources解锁本金。
Outcome<bool> naval_repair_charge_service_current(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalRepairChargeTerms&);
Status append_naval_repair_charge_payment(const CommonRead&,const NavalRepairChargeTerms&,const StableId& action_id,ResourceRecipe&);
}

// 只重放原初成员/具名入会/回应事实的截面；不持第二成员账，不从今天的态度倒推旧签字。
#pragma once
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
struct PoliticalShopLicensingContext;struct ActingAuthority;
Status validate_political_shop_historical_member_duty(const PoliticalShopLicensingContext&,const ActingAuthority&,Integer tick,const StableId& actual_legal_fact);
}

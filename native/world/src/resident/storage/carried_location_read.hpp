// 只读实际 S1/S2 双向依附来源；不以字符串或 Owner 代替乘载绑定。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
class CarriedLocationValidationRead {
public:
 virtual ~CarriedLocationValidationRead()=default;
 virtual Status validate_actor_carried_location(const StableId& actor_id,const CarriedLocation&)const=0;
};
}

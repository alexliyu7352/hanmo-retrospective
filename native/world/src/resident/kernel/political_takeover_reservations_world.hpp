// 逐预约由原Task或赈济唯一域证明；返回S2具名proof，不提供任意公库放行bool。
#pragma once
#include "resident/domains/military/occupation_reservations.hpp"
namespace hanmo::resident {
struct WorldBundle;struct DynamicStepInput;
OccupationReservationRead political_takeover_reservation_read(const WorldBundle&,const DynamicStepInput* phase=nullptr);
}

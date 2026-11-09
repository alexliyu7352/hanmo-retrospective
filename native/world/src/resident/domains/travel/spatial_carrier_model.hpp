// S1真实载体身份；人、军团、船队和运输编组不互造替身。
#pragma once
#include "../../model/dynamic_contract.hpp"
namespace hanmo::resident {
struct TransportUnitRef { SubjectRef subject; StableId transport_unit_id; };
struct ActorCarrierRef { StableId actor_id; };
struct PartyCarrierRef { StableId party_id; };
struct ArmyCarrierRef { StableId army_id; };
struct FleetCarrierRef { StableId fleet_id; };
struct TransportUnitCarrierRef { TransportUnitRef transport_unit_ref; Integer composition_revision=0; };
using SpatialCarrierRef=std::variant<ActorCarrierRef,PartyCarrierRef,ArmyCarrierRef,FleetCarrierRef,TransportUnitCarrierRef>;
// 每个lot只选择一次，重量/数量由Common真库存读，不存第二物资账。
// CargoSelection由Common唯一声明；S1只读其精确lot选择，不另造货物写型。
} // 实际军事/人口/逐船成员仍由各原owner提供。

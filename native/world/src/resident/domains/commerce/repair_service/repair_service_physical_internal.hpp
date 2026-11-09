#pragma once
#include "repair_service_physical.hpp"
#include "repair_service_agreement_internal.hpp"
namespace hanmo::resident::repair_physical_detail {
using namespace repair_service_detail;
const RepairItemIndependentSourceRead& independent(const RepairPhysicalReadContext&);
const RepairJob* job(const RepairServiceReadContext&,const RepairServiceContractSource&);
struct ReturnTerminal {const RepairJob*job=nullptr;MomentOwnerSource source;WorldMoment at;};
ReturnTerminal terminal(const RepairServiceReadContext&,const RepairServiceContractSource&);
void contact(const RepairPhysicalReadContext&,const RepairServiceContractSource&,const StableId&,const MomentOwnerSource&,const WorldMoment&,const SourceRef*,bool);
void transfer_shape(const RepairServiceReadContext&,const RepairServiceContractSource&,const OriginalUniqueItemTransferEffect&,const CargoSelection&,const ItemConditionHead&,const WorldMoment&,const SourceRef*);
}

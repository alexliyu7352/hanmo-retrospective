// Original physical sources. Common remains the only lot/title mutation engine.
#pragma once
#include "repair_service_native.hpp"
namespace hanmo::resident {
// This dependency must reconstruct original title, both inventories' actual
// spatial reachability/capacity and the permitted exclusive-use reservation at
// the source cut. A saved effect, quote or today's Owner is not that proof.
// It must not call the complete S7 validator or this physical adapter again.
class RepairItemIndependentSourceRead {public:virtual ~RepairItemIndependentSourceRead()=default;
 virtual bool bound_to(const CommonStore&,const ItemInstancesState&)const noexcept=0;
 virtual Status validate_original_repair_item_physical_scope(const RepairServiceContractSource&,
  const OriginalUniqueItemTransferEffect&,const StableId&actual_actor,const MomentOwnerSource&contact,
  const WorldMoment&,const SourceRef*actual_cut,bool returning)const=0;
 virtual Status visit_original_repair_item_physical_pins(const RepairServiceContractSource&,
  const OriginalUniqueItemTransferEffect&,const WorldMoment&,const SourceRef&actual_cut,bool returning,RepairServiceSourcePinSink&)const=0;
 virtual Outcome<MomentOwnerSource> read_original_repair_return_destination(const RepairServiceContractSource&,
  const StableId&destination_inventory,const WorldMoment&,const SourceRef*actual_cut)const=0;
};
struct RepairPhysicalReadContext {
 const RepairServiceAgreementReadContext&agreements;
 const RepairServiceReadContext&commercial;
 const RepairItemIndependentSourceRead*independent=nullptr;
};
class NativeRepairServicePhysicalOriginalRead final:public RepairServicePhysicalOriginalRead {
 const RepairPhysicalReadContext&x_;
public:explicit NativeRepairServicePhysicalOriginalRead(const RepairPhysicalReadContext&x):x_(x){}
 bool bound_to(const CommonStore&,const ItemInstancesState&)const noexcept override;
 Status validate_original_repair_custody(const RepairServiceContractSource&,const RepairCustodyReceivedSource&)const override;
 Outcome<std::vector<MomentOwnerSource>>read_original_repair_terminal_continuity(const RepairServiceContractSource&,const MomentOwnerSource&)const override;
 Status validate_original_repair_return(const RepairServiceContractSource&,const RepairItemReturnedSource&)const override;
};
SourceRef repair_custody_item_transfer_source(const StableId&custody_id);
SourceRef repair_return_item_transfer_source(const StableId&return_id);
Outcome<const RepairCustodyReceivedSource*>read_repair_custody_received(const RepairServiceReadContext&,const MomentOwnerSource&);
Outcome<const RepairItemReturnedSource*>read_repair_item_returned(const RepairServiceReadContext&,const MomentOwnerSource&);
// Owned-field identity only: there is deliberately no Common Fact at this ref.
Outcome<const OriginalUniqueItemTransferEffect*>read_repair_original_item_transfer(const RepairServiceReadContext&,const MomentOwnerSource&);
Status validate_repair_service_physical_sources(const RepairServiceReadContext&);
Status visit_repair_service_physical_pins(const RepairPhysicalReadContext&,const MomentOwnerSource&,RepairServiceAgreementPinSink&);
}

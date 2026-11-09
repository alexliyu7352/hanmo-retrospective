#pragma once
#include "household_mobile_custody.hpp"
#include "household_purchase_cargo.hpp"
#include "household_buyer_execution.hpp"
#include "resident/domains/travel/group_transport_registration.hpp"
namespace hanmo::resident {
struct HouseholdPurchaseTransportContext;struct GroupMovementExecutionState;
// Original plan acceptance supplies the finite approach and return scope. S1
// still produces the one registration/materialization and the actual release.
class NativeHouseholdTransportRegistrationBusinessRead final:public TransportRegistrationBusinessRead {
 const HouseholdPurchaseContext& x_;const SpatialCarrierState& carriers_;
 const TransportRegistrationState& registrations_;const SpatialCarrierSources& spatial_;
 const MovementExactState& movement_;const GroupMovementExecutionState& executions_;
public:
 NativeHouseholdTransportRegistrationBusinessRead(const HouseholdPurchaseContext& x,
  const SpatialCarrierState& c,const TransportRegistrationState& r,const SpatialCarrierSources& s,
  const MovementExactState& m,const GroupMovementExecutionState& e)
  :x_(x),carriers_(c),registrations_(r),spatial_(s),movement_(m),executions_(e){}
 Outcome<TransportRegistrationAuthority> read_registration_authority(const SourceRef&)const override;
 Status validate_registration_authority(const TransportRegistrationAuthority&)const override;
 Status validate_registered_initiator(const TransportRegistrationSource&,const TransportUnitState&,const GroupInitiator&,const SubjectRef&,const WorldMoment&)const override;
 Outcome<std::vector<StableId>> read_releasable_labor_allocations(const StableId&,const SourceRef&)const override;
 Status validate_transport_handover(const TransportUnitReleaseSource&,const TransportUnitState&)const override;
};
}

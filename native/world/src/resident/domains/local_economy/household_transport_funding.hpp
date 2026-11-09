#pragma once
#include "household_transport_read.hpp"
#include "resident/domains/travel/group_transport_registration.hpp"
namespace hanmo::resident {
// The policy remains S5; S1's actual registration/journey is the bounded acceptance.
// No retained balance, route-quote commitment, or household SubjectRef is introduced.
class NativeHouseholdTransportExpenseAuthorityRead final:public TransportExpenseAuthorityRead {
 const HouseholdPurchaseContext& x_;
 const SpatialCarrierState& carriers_;
 const TransportRegistrationState& registrations_;
 const MovementExactState& movement_;const GroupMovementExecutionState& executions_;const SpatialCarrierSources& spatial_;const TransportExpenseReceiverRead* receiver_;
public:
 NativeHouseholdTransportExpenseAuthorityRead(const HouseholdPurchaseContext& x,const SpatialCarrierState& carriers,const TransportRegistrationState& registrations,const MovementExactState& movement,const GroupMovementExecutionState& executions,const SpatialCarrierSources& spatial,const TransportExpenseReceiverRead* receiver=nullptr):x_(x),carriers_(carriers),registrations_(registrations),movement_(movement),executions_(executions),spatial_(spatial),receiver_(receiver){}
 Outcome<TransportExpenseAllowance> read_transport_expense_allowance(const TransportExpenseAuthority&,const ActualTransportExpense&,TransportExpenseAuthorityCheck=TransportExpenseAuthorityCheck::new_charge)const override;
 Outcome<TransportExpenseResponsibility> read_original_transport_responsibility(const TransportExpenseAuthority&,const ActualTransportExpense&,TransportExpenseAuthorityCheck)const override;
};
}

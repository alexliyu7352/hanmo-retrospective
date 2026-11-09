// Borrowed original S7 finance for the canonical S9 RepairService projection.
#pragma once
#include "repair_service.hpp"
namespace hanmo::resident {
class RepairServiceFinancialSourceVisitor {
public:virtual ~RepairServiceFinancialSourceVisitor()=default;
 virtual Status contract(const RepairServiceContractSource&,const RepairAcceptedSource&)=0;
 virtual Status charge(const RepairServiceChargeSource&)=0;
 virtual Status payment(const RepairServicePaymentSource&,const PaymentReceipt&)=0;
 virtual Status settlement(const RepairServiceTerminationSettlementSource&)=0;
};
// The real canonical acceptance is required. Callback arguments are borrowed
// only during this call; it neither writes money nor triggers termination.
// Complete actual financial validation precedes callbacks, including unpaid
// original earned debt. No current wallet total is used to invent a leg.
Status visit_original_repair_service_financial_sources(const RepairServiceReadContext&,
 const MomentOwnerSource& actual_repair_acceptance,RepairServiceFinancialSourceVisitor&);
}

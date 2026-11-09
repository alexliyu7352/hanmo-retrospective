#pragma once
#include "obligation_sources_read.hpp"
namespace hanmo::resident {
class EconomicObligationSourcePinSink {public:virtual ~EconomicObligationSourcePinSink()=default;
 virtual Status original_source(const MomentOwnerSource&)=0;
 virtual Status obligation(const ObligationRef&)=0;
 virtual Status original_arrear(const SourceRef&)=0;
 virtual Status original_cash_payment(const SourceRef&)=0;
 virtual Status investment_contract(const InvestmentContractRef&)=0;
 virtual Status domain_read_version(const DomainReadVersion&)=0;
 virtual Status responsibility_epoch(const ObligationResponsibilityEpochRef&)=0;
};
// The full owner read runs first. P11 rows are owned subrecords, never invented
// Common facts. Explicit arrear/payment/epoch methods preserve those identities.
Status visit_economic_obligation_source_pins(const EconomicObligationReadContext&,EconomicObligationSourcePinSink&);
}

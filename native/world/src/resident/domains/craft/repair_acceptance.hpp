// E16.7/I25.5: actual Repair acceptance is S9; no future Action is fabricated.
#pragma once
#include "repair_contract.hpp"
namespace hanmo::resident {
struct RepairAcceptanceQuote {
 RepairAcceptedTerms terms;WorldIdentity identity;WorldMoment quoted_at;ReadVersion actual_inventory_version;
};
struct RepairAcceptedServiceView {
 const RepairAcceptedSource* acceptance=nullptr;
 const RepairJob* started_job=nullptr;
 const RepairUnstartedInterruptionSource* unstarted_interruption=nullptr;
};
SourceRef repair_accepted_source(const StableId&);
SourceRef repair_unstarted_interrupted_source(const StableId&);
Outcome<RepairAcceptanceQuote> preview_repair_acceptance(const RepairReadContext&,const RepairAcceptedTerms&);
Outcome<SourceRef> accept_repair_service_in_candidate(CommonStore&,ItemInstancesState&,const RepairReadContext&,const RepairAcceptanceQuote&);
// The whole accepted service can become genuinely impossible before Start.
// This records only that original stop. It does not release custody, refund,
// invent provided work, consume material, or install an Action/Facility claim.
Outcome<SourceRef> interrupt_unstarted_repair_in_candidate(CommonStore&,ItemInstancesState&,const RepairReadContext&,const MomentOwnerSource& accepted_service,const MomentOwnerSource& actual_cause);
Outcome<RepairAcceptedServiceView> read_repair_accepted_service(const RepairReadContext&,const MomentOwnerSource&);
Outcome<const RepairUnstartedInterruptionSource*> read_repair_unstarted_interruption(const RepairReadContext&,const MomentOwnerSource&);
Outcome<WorldMoment> read_repair_acceptance_source_moment(const RepairReadContext&,const SourceRef&,Integer);
Status validate_repair_start_acceptance(const RepairReadContext&,const RepairTerms&,const RepairJob*);
Status validate_repair_acceptances(const RepairReadContext&);
class RepairAcceptancePinSink {public:virtual ~RepairAcceptancePinSink()=default;
 virtual Status source(const MomentOwnerSource&)=0;
 virtual Status item_condition(const ItemConditionHead&)=0;
};
// Outgoing retained dependencies only: acceptance never pins its later Start
// or terminal, which themselves retain the actual acceptance.
Status visit_repair_acceptance_pins(const RepairReadContext&,const MomentOwnerSource&,RepairAcceptancePinSink&);
Status visit_repair_unstarted_interruption_pins(const RepairReadContext&,const MomentOwnerSource&,RepairAcceptancePinSink&);
}

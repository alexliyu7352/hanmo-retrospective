// Borrowed original S9 evidence for the one shared RepairService address.
#pragma once
#include "repair_acceptance.hpp"
namespace hanmo::resident {
struct RepairService;struct RepairTerminal;
struct RepairCompletionFinancialEvidence {
 const RepairJob* job=nullptr;const RepairCompletedSource* completion=nullptr;
};
// This named dependency cut verifies the original completion, service and
// consumed material, but does not recursively read the money resulting from
// that very completion. Full Repair publication validates those money sources.
Outcome<RepairCompletionFinancialEvidence> read_repair_completion_for_financial_evidence(
 const RepairReadContext&,const MomentOwnerSource&);
struct RepairOriginalServiceTermination {
 const RepairAcceptedSource* acceptance=nullptr;
 const RepairJob* started_job=nullptr;
 const RepairUnstartedInterruptionSource* unstarted=nullptr;
 const RepairInterruptedSource* started=nullptr;
};
Outcome<const RepairAcceptedSource*> read_repair_service_scope(
 const RepairReadContext&,const RepairService&);
// Exactly one original internal-unavailability result. Actual completion is
// not reclassified as an interrupted whole service, and no fee/refund is made.
Outcome<RepairOriginalServiceTermination> read_repair_service_internal_termination(
 const RepairReadContext&,const RepairService&,const RepairTerminal&);
Outcome<const RepairInterruptedSource*> read_repair_interruption_source(
 const RepairReadContext&,const MomentOwnerSource&);
}

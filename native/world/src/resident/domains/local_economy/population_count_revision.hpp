// The original count writer has already applied this receipt in the candidate.
// These hooks record only its P02 revision dependency, never another population
// delta, acceptance, location, qualification or allocation.
#pragma once
#include "population_transport.hpp"
namespace hanmo::resident {
Status record_population_transfer_revisions_in_candidate(
 const CommonStore&,const LocalEconomyState&,PopulationTransportState&,
 const StableId& actual_transfer_id,const PopulationTransferLaborCapture* actual_labor_capture=nullptr);
Status record_population_loss_revision_in_candidate(
 const CommonStore&,const LocalEconomyState&,PopulationTransportState&,
 const StableId& actual_loss_id);
// The original multi-member result passes exactly its actual receipt IDs, in
// writer order. Full source coverage is checked after the whole batch. Pending
// selections are reconciled by their original owners before stable validation;
// a reservation cannot prevent the actual casualty from recording its count.
Status record_population_count_receipt_revisions_in_candidate(
 const CommonStore&,const LocalEconomyState&,PopulationTransportState&,
 const std::vector<StableId>& actual_receipt_ids,const std::vector<PopulationTransferLaborCapture>* actual_labor_captures=nullptr);
// Bidirectional original count/source coverage, including already released
// selections and later history. No restore-time repair or source synthesis.
Status validate_population_count_revisions(
 const CommonStore&,const LocalEconomyState&,const PopulationTransportState&);
}

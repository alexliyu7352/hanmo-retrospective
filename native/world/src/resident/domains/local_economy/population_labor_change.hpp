// Original allocation mutations, owned by the same population revision source.
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
struct PopulationLaborChange {
 StableId allocation_id;
 Integer before_workers=0,after_workers=0,before_through_tick=0,after_through_tick=0;
};
// Ephemeral output of the actual count writer. It has no decoder or setter.
// The receiving P02 hook binds it to the same successful transfer candidate.
struct PopulationTransferLaborCaptureWriter;
class PopulationTransferLaborCapture {
 friend struct PopulationTransferLaborCaptureWriter;
 StableId transfer_id_,cohort_id_;
 std::vector<PopulationLaborChange> changes_;
public:
 const StableId& transfer_id()const{return transfer_id_;}
 const StableId& cohort_id()const{return cohort_id_;}
 const std::vector<PopulationLaborChange>& changes()const{return changes_;}
};
}

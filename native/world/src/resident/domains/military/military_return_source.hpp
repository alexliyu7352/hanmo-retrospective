#pragma once
#include "military_action_time_records.hpp"
#include "resident/storage/common_store.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {

struct MilitaryCompletedReturnSourceBuildView {
 const DynamicMilitaryReturnFact2* original=nullptr;
 MomentOwnerSourceBuildView actual_return,actual_application;
};

struct MilitaryCompletedReturnSource {
 CheckpointMemoryCharge copied_fields;
 const DynamicMilitaryReturnFact2* original=nullptr;
 MomentOwnerSource actual_return;
 MomentOwnerSource actual_application;
 MilitaryCompletedReturnSource()=default;
 explicit MilitaryCompletedReturnSource(const MilitaryCompletedReturnSourceBuildView&);
 MilitaryCompletedReturnSource(const MilitaryCompletedReturnSource&)=default;
 MilitaryCompletedReturnSource(MilitaryCompletedReturnSource&&)noexcept=default;
 MilitaryCompletedReturnSource& operator=(const MilitaryCompletedReturnSource&)=delete;
 MilitaryCompletedReturnSource& operator=(MilitaryCompletedReturnSource&&)=delete;
};
// Source-local completion proof. It checks the real Return/Action rows, complete
// S0 service and both Common heads without re-entering spatial or Army validators.
class MilitaryReturnSourceRead {
public:
 virtual ~MilitaryReturnSourceRead()=default;
 virtual bool bound_to(const CommonStore&,const MilitaryState&)const noexcept=0;
 // null means this retained Army has no exact return row. Legacy return sources
 // remain with their original owner; malformed exact rows never become null.
 virtual Outcome<std::optional<MilitaryCompletedReturnSource>> read_exact_army_return(const StableId& army_id)const=0;
};
}

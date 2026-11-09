#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {

struct MilitaryFormationSourceBuildView {
 const ArmyFormationTerms* original_terms=nullptr;
 MomentOwnerSourceBuildView completed_source;
 SourceRefBuildView original_action_source;
 const WorldMoment& completed_at;const Integer& original_completion_serial;
};

struct MilitaryFormationSourceView {
 CheckpointMemoryCharge copied_fields;
 const ArmyFormationTerms* original_terms=nullptr;
 MomentOwnerSource completed_source;
 SourceRef original_action_source;
 WorldMoment completed_at;
 Integer original_completion_serial=0;
 MilitaryFormationSourceView()=default;
 explicit MilitaryFormationSourceView(const MilitaryFormationSourceBuildView&);
 MilitaryFormationSourceView(const MilitaryFormationSourceView&)=default;
 MilitaryFormationSourceView(MilitaryFormationSourceView&& other):MilitaryFormationSourceView(static_cast<const MilitaryFormationSourceView&>(other)){}
 MilitaryFormationSourceView& operator=(const MilitaryFormationSourceView&)=delete;
 MilitaryFormationSourceView& operator=(MilitaryFormationSourceView&&)=delete;
};
// Borrowed source-local identity and actual completed work only. Full acceptance
// and participation history is independently validated by the original owner.
// This interface cannot recurse into roster/spatial whole-world validators.
class MilitaryFormationSourceRead {
public:
 virtual ~MilitaryFormationSourceRead()=default;
 virtual bool bound_to(const CommonState&,const MilitaryState&)const noexcept=0;
 virtual Outcome<std::optional<MilitaryFormationSourceView>> read_exact_army_formation(const StableId& army_id)const=0;
};
// Unique old-or-exact source, no synthetic legacy row and no current roster copy.
Outcome<MilitaryFormationSourceView> read_original_military_formation(
 const CommonState&,const MilitaryState&,const ArmyRecord&,const WorldMoment& actual_now,
 const MilitaryFormationSourceRead* exact=nullptr,ExactArithmeticBudget={});
}

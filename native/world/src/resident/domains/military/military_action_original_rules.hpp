#pragma once
#include "military_model.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <string_view>
namespace hanmo::resident {

struct OriginalOfficeActionCompletionBuildView {
 bool success=true;
 std::string_view code,office_id;
};

// This is the original Office business result's storage owner, not another
// completion source or a lease for subsequent caller writes and outward copies.
struct OriginalOfficeActionCompletion {
private:
 static CheckpointMemoryCharge reserve(const OriginalOfficeActionCompletionBuildView&);
 static CheckpointMemoryCharge reserve(const OriginalOfficeActionCompletion&);
public:
 CheckpointMemoryCharge copied_fields;
 MilitaryEffect effect;
 OriginalOfficeActionCompletion();
 explicit OriginalOfficeActionCompletion(const OriginalOfficeActionCompletionBuildView&);
 OriginalOfficeActionCompletion(const OriginalOfficeActionCompletion&);
 OriginalOfficeActionCompletion(OriginalOfficeActionCompletion&& other)
  :OriginalOfficeActionCompletion(static_cast<const OriginalOfficeActionCompletion&>(other)){}
 OriginalOfficeActionCompletion& operator=(const OriginalOfficeActionCompletion&)=delete;
 OriginalOfficeActionCompletion& operator=(OriginalOfficeActionCompletion&&)=delete;
};
class MilitaryActionResourceCapture;
class MilitaryActionCompletionConditionGuard;
// Original ready-body leaves shared by live predicates and source-backed cuts.
// Battle, action occupation, place and authority remain their original callers.
bool original_military_actor_life_ready(bool alive,bool available,Integer hp)noexcept;
bool original_military_actor_injury_ready(Integer injury)noexcept;
// Original OfficeTerms/contact/readiness/appointment body. The exact caller
// must first prove its real full service gate; legacy keeps its original tick
// gate. Neither path owns a second Office or a separate appointment result.
Outcome<OriginalOfficeActionCompletion> complete_original_office_action_in_candidate(
 CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,
 const ActionCoreView&,const OfficeTerms&,const std::optional<InteractionContext>&,const MilitaryActionCompletionConditionGuard* condition_guard=nullptr);
// These are the original pure Formation derivations. The live preview and
// historical Original reader share them; neither supplies authority or time.
void append_original_formation_good(std::vector<std::pair<StableId,Integer>>&,const StableId&,Integer);
Outcome<Integer> append_original_formation_unit_equipment(const MilitaryCatalog&,
 const MilitaryUnitRecord&,Integer additional_retainers,std::vector<std::pair<StableId,Integer>>&);
Outcome<Integer> read_original_formation_food_quantity(const GarrisonDefinition&,
 Integer mouths,const std::optional<Integer>& requested,bool valid,Integer money,bool money_valid);
Integer original_formation_goods_weight_after(Integer accumulated,Integer quantity,Integer item_weight)noexcept;
class MilitaryFormationReservationCapture;
Status reserve_original_army_formation_in_candidate(CommonWrite&,MilitaryState&,const StableId& action_id,const ArmyFormationTerms&,MilitaryFormationReservationCapture* capture=nullptr);
Outcome<MilitaryEffect> complete_original_army_formation_in_candidate(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const ActionCoreView&,const ArmyFormationTerms&,Integer actual_supply_period_to,MilitaryActionResourceCapture* resource_capture=nullptr,const MilitaryActionCompletionConditionGuard* condition_guard=nullptr);
}

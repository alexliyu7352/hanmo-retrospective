#pragma once
#include "military_action_resource_model.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
// Same original same-candidate capture header, borrowed without copying a draft.
Outcome<const ResourceExecutionDraft*> read_original_military_resource_capture(const CommonStore&,const CommonWrite&,const std::shared_ptr<const ResourceExecutionCaptureIdentity>&,std::size_t first);
// Same-stack original completion seam. It captures precisely the sole success
// recipe before unrelated callbacks/cleanup. Nothing here is a persisted root.
class MilitaryActionResourceCapture {
 CheckpointMemoryCharge opening_memory_{0};
 const CommonStore* common_;
 std::shared_ptr<const ResourceExecutionCaptureIdentity> capture_;
 std::size_t first_;
 std::optional<MilitaryReturnResourceOpening1> opening_;
 std::optional<MilitaryActionResourceEffect1> effect_;
public:
 explicit MilitaryActionResourceCapture(const CommonStore&);
 MilitaryActionResourceCapture(const MilitaryActionResourceCapture&)=delete;
 MilitaryActionResourceCapture(MilitaryActionResourceCapture&&)=default;
 MilitaryActionResourceCapture& operator=(MilitaryActionResourceCapture&&)=delete;
 Status capture_return_opening(const CommonWrite&,const InventoryView&,const InventoryView* destination_without_transfer);
 Status capture_original_success(const CommonWrite&);
 const std::optional<MilitaryActionResourceEffect1>& effect()const noexcept{return effect_;}
};
Outcome<std::optional<MilitaryActionResourceCapture>> begin_military_action_resource_capture(const CommonStore&,Integer selected_schema);
struct MilitaryState;struct MilitaryCatalog;struct DynamicMilitaryActionFact2;
struct DynamicMilitaryLogisticsFact2;struct DynamicMilitaryReturnFact2;
// Source-local original recipe/result correspondence. These do not replace
// historical title, original acceptance or whole-service proof.
Status validate_original_formation_resource_result(const MilitaryState&,const MilitaryCatalog&,const DynamicMilitaryActionFact2&,Integer selected_schema);
Status validate_original_return_resource_result(const MilitaryState&,const MilitaryCatalog&,const CommonCatalog*,const DynamicMilitaryLogisticsFact2&,const DynamicMilitaryReturnFact2&,Integer selected_schema);
}

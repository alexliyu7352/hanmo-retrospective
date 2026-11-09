// S1实际边界及PassageRequest/3；准入/征费权唯一属于S6，不由路线可达推定。
#pragma once
#include "movement_exact_model.hpp"
#include <set>
namespace hanmo::resident {
struct ReadStamp;
enum class PassageBoundaryKind { road_edge,gate,ferry,port };
struct PassageBoundaryRef { PassageBoundaryKind kind=PassageBoundaryKind::road_edge; StableId id,map_id,from_site_id,to_site_id; };
struct PassageWindow { Integer from_tick=0,until_tick=0; };
struct ActorPassageRequester { StableId acting_actor_id; };
struct HouseholdPolicyPassageRequester { StableId household_id,accepted_household_policy_ref; Integer policy_revision=0; StableId purchase_plan_ref; std::optional<StableId> purchase_ref; StableId phase; };
using PassageRequester=std::variant<ActorPassageRequester,HouseholdPolicyPassageRequester>;
struct PassageManifestBasis { Integer manifest_revision=0,composition_revision=0; std::vector<SourceRef> source_refs; };
struct PassageCargoSummary { StableId cargo_class; Integer quantity=0,weight=0; };
enum class PassagePurpose { preview,enter };
struct OrdinaryPassageContext {};
struct SafeExitPassageContext {StableId binding_id;std::vector<StableId> line_refs;StableId path_id;Integer next_boundary_ordinal=0;};
using PassagePurposeContext=std::variant<OrdinaryPassageContext,SafeExitPassageContext>;
// Structural only. S6 must prove the actual entitlement, path and live window.
inline Status validate_passage_purpose_context_shape(const PassagePurposeContext& context){if(const auto* exit=std::get_if<SafeExitPassageContext>(&context)){if(exit->binding_id.empty()||exit->path_id.empty()||exit->line_refs.empty()||exit->next_boundary_ordinal<1||exit->next_boundary_ordinal>64)return common_failure("InvalidSafeExitPassageContext");std::set<StableId> seen;for(const auto& id:exit->line_refs)if(id.empty()||!seen.insert(id).second)return common_failure("InvalidSafeExitPassageLine",id);}return common_success();}
struct PassageRequest {
    WorldIdentity identity; std::shared_ptr<const ReadStamp> read_stamp;
    SubjectRef subject; PassageRequester requester; SpatialCarrierRef carrier;
    PassageManifestBasis manifest_basis; PassageBoundaryRef boundary; PassageWindow window;
    StableId mode; std::optional<StableId> army_id,fleet_id;
    std::vector<PassageCargoSummary> cargo_summary; PassagePurpose purpose=PassagePurpose::preview;
    std::optional<MovementExecutionRef> execution;
    PassagePurposeContext purpose_context=OrdinaryPassageContext{};
};
// 唯一跨界源只记录已实际采用的决定和付款；原S6决定仍在原域保存。
struct BoundaryCrossingReceipt {
    StableId receipt_id; MovementExecutionRef execution; SpatialCarrierRef carrier;
    Integer composition_revision=0,manifest_revision=0; PassageBoundaryRef boundary;
    WorldMoment entry_at;Integer entry_ordinal=0; SourceRef decision_source_ref;
    std::vector<SourceRef> payment_refs,effect_use_refs; std::optional<SourceRef> safe_exit_binding_ref;
};
Status visit_movement_execution_ref(CodecVisitor&,const char* name,MovementExecutionRef&);
Status visit_spatial_carrier_ref(CodecVisitor&,const char* name,SpatialCarrierRef&);
Status visit_passage_boundary_ref(CodecVisitor&,const char* name,PassageBoundaryRef&);
Status visit_passage_requester(CodecVisitor&,const char* name,PassageRequester&);
} // Request是热拥有值，ReadStamp不进入持久源；冷receipt沿真实源闭包反验。

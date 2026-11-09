// C9.10 / maritime B1: S1 owns actual birth placement and transport attachment.
// A query allocates neither a child nor a source. S4 owns the actual due/birth.
#pragma once
#include "spatial_location_capture.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct BirthPlacementQuery {
 StableId delivery_actor_id,birth_arrangement_id;
 const ReadStamp& read_stamp;
};
enum class BirthPlacementKind {settlement,road,wilderness,fleet,custody};
struct BirthPlacement {
 Location location;
 MovementAnchor resolved_anchor;
 BirthPlacementKind placement_kind=BirthPlacementKind::settlement;
 std::optional<SpatialCarrierRef> carrier_ref;
 std::optional<MomentOwnerSource> safe_boundary_ref;
 MomentOwnerSource placement_basis_ref;
};
enum class TravelDependentPolicy {follow_carrier_until_explicit_handover};
// Time2 travel_dependents/2: the original attached_tick slot is an actual M.
// transport_ref identifies the original transport source, including the real
// vessel/passenger source where applicable; it is not a second carrier record.
struct TravelDependentBinding {
 StableId binding_id,dependent_actor_id,carrier_actor_id;
 std::optional<MomentOwnerSource> transport_ref;
 MomentOwnerSource source_birth_ref;
 WorldMoment attached_at;
 TravelDependentPolicy policy=TravelDependentPolicy::follow_carrier_until_explicit_handover;
 Integer revision=0;
};
struct BirthPlacementAdoption {
 StableId delivery_actor_id,birth_arrangement_id,child_actor_id;
 MomentOwnerSource actual_birth_source;
 WorldMoment occurred_at;
 const BirthPlacement& actual_placement;
};
struct BirthPlacementApplication {
 MomentOwnerSource location_result;
 std::optional<StableId> transport_binding_ref;
 std::vector<BoundSpatialLocationWrite> actual_writes;
 bool already_applied=false;
};
struct BirthPlacementApplicationRead {
 const BirthPlacement* actual_placement=nullptr;
 StableId delivery_actor_id,birth_arrangement_id,child_actor_id;
 WorldMoment occurred_at;
 MomentOwnerSource actual_birth_source,location_result;
 std::optional<StableId> transport_binding_ref;
};
// Every reachable kind must have its real owner adapter at compose. The result
// is read from the delivery actor's original placement/carrier/custody sources.
// Historical validation uses the actual birth cut and retained source chain;
// it may not re-query today's delivery actor or select a desired location.
class BirthPlacementRead {
public:
 virtual ~BirthPlacementRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Status require_birth_placement_kind(BirthPlacementKind)const=0;
 virtual Outcome<BirthPlacement> read_birth_placement(const BirthPlacementQuery&)const=0;
 virtual Outcome<BirthPlacementApplicationRead> read_birth_placement_application(
   const MomentOwnerSource& actual_birth_source,const MomentOwnerSource& location_result)const=0;
 virtual Outcome<BirthPlacementApplicationRead> read_birth_placement_result(
   const MomentOwnerSource& location_result)const=0;
 virtual Outcome<BirthPlacementApplicationRead> read_birth_placement_for_child(const StableId& child_actor_id)const=0;
 virtual Status validate_birth_placement_at_birth(const StableId& delivery_actor_id,
   const StableId& birth_arrangement_id,const MomentOwnerSource& actual_birth_source,
   const WorldMoment&,const BirthPlacement&)const=0;
};
// S4 first creates the true child/birth source in the same outer candidate. S1
// then adopts that placement and any necessary dependent relation. Fleet load,
// care and provision remain with their original owners. A failure discards the
// entire candidate; no internal commit or compensating teleport is permitted.
// The concrete implementation must bind every successful Common insertion or
// location mutation to the existing candidate-wide spatial capture.
class BirthPlacementWrite:public BirthPlacementRead {
public:
 ~BirthPlacementWrite()override=default;
 virtual Outcome<BirthPlacementApplication> adopt_birth_placement_in_candidate(
   CommonStore&,const BirthPlacementAdoption&)=0;
 virtual Status validate_birth_placement_application(const BirthPlacementAdoption&,
   const MomentOwnerSource& location_result,const std::optional<StableId>& transport_binding_ref)const=0;
};
}

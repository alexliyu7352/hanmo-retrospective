// 原制作参与者的位置采用只存既有S1来源引用；实际服务须逐人覆盖完整半开窗。
#pragma once
#include "craft_job.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <cstddef>
#include <vector>
#include "resident/domains/travel/actor_spatial_history.hpp"
namespace hanmo::resident {
struct CraftSpatialSourceContext {
 const CraftJobReadContext& craft;
 const TravelCatalog& travel_catalog;
 const MovementExactState& movement;
 const SpatialCarrierState& carriers;
 const TransportRegistrationState& registrations;
 const AuthoredActorFoundationState& foundation;
 const ActorFoundationCatalog& actor_catalog;
 const SpatialCarrierSources& sources;
 const MilitarySpatialHandoffState* military_handoffs=nullptr;
 const FamilyCatalog* family_catalog=nullptr;
 const BirthPlacementRead* births=nullptr;
 const MilitaryActionPlacementRead* military_actions=nullptr;
};
// 只在真正span.from捕获当刻位置；不将计划中的未来区间当作已提供服务。
class CapturedCraftParticipantSpatialInputs final {
 CheckpointMemoryCharge copied_fields_;
 std::vector<MomentOwnerSource> sources_;
 std::size_t capacity_=0;
 static CheckpointMemoryCharge storage_memory(std::size_t);
 explicit CapturedCraftParticipantSpatialInputs(std::size_t);
 void append(const SourceRef&,Integer);
 friend Outcome<CapturedCraftParticipantSpatialInputs> capture_craft_participant_spatial_inputs(const CraftSpatialSourceContext&,const CraftJob&,const WorkServiceIntervalProof&);
public:
 CapturedCraftParticipantSpatialInputs()noexcept=default;
 ~CapturedCraftParticipantSpatialInputs()=default;
 CapturedCraftParticipantSpatialInputs(const CapturedCraftParticipantSpatialInputs&)=delete;
 CapturedCraftParticipantSpatialInputs& operator=(const CapturedCraftParticipantSpatialInputs&)=delete;
 CapturedCraftParticipantSpatialInputs(CapturedCraftParticipantSpatialInputs&&)noexcept=default;
 CapturedCraftParticipantSpatialInputs& operator=(CapturedCraftParticipantSpatialInputs&&)=delete;
 // The role count bounds storage, not the final number of distinct heads.
 // Borrow only while this exact capture output owner remains alive.
 const std::vector<MomentOwnerSource>& sources()const noexcept{return sources_;}
};
Outcome<CapturedCraftParticipantSpatialInputs> capture_craft_participant_spatial_inputs(
 const CraftSpatialSourceContext&,const CraftJob&,const WorkServiceIntervalProof&);
bool is_craft_participant_spatial_source(const MomentOwnerSource&)noexcept;
Status validate_prepared_craft_participant_spatial_source(
 const CraftSpatialSourceContext&,const CraftJob&,const WorkServiceIntervalProof&,const MomentOwnerSource&);
// 实际to及冷读都沿同一已采用源核位置与原工位；不从今日位置倒填旧段。
Status validate_craft_participant_spatial_span(
 const CraftSpatialSourceContext&,const CraftJob&,const WorkServiceIntervalProof&);
}

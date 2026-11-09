// Borrow the actual Family/body holders; never recursively compose a world here.
#pragma once
#include "visit_reply_life.hpp"
#include "visit_reply_authored.hpp"
#include "visit_reply_read.hpp"
#include "resident/domains/interaction/occupancy/actor_social_occupancy_initial.hpp"
#include "resident/domains/actor_foundation/character_condition_world.hpp"
#include "resident/domains/family/profile_control_eligibility.hpp"
#include "resident/kernel/world_read_stamp.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
class NativeVisitReplyLifeRead final:public CheckpointAllocatedObject {
 const WorldBundle& world_;const CharacterConditionWorldRead& body_owner_;
 const FamilyContinuationPeopleRead4& people_;const ProfileControlEligibilitySources& controls_;
 WorldCurrentMomentRead clock_;std::optional<VisitAuthoredCommandRead> commands_;FamilyDeathOriginRead legacy_deaths_;
 std::optional<NativeActorSocialOccupancyInitialRead> occupancy_initial_;
 std::optional<NativeActorSocialOccupancyRead> occupancy_;
 VisitMessageLifeContext messages_;std::optional<VisitReplyAuthorshipRead> authorship_;
 VisitReplyLifeContext life_;VisitReplyReadContext context_;
 NativeVisitReplyLifeRead(const WorldBundle&,const CharacterConditionWorldRead&,const CharacterConditionSources&,const FamilyContinuationPeopleRead4&,const ProfileControlEligibilitySources&);
public:
 static Outcome<std::unique_ptr<NativeVisitReplyLifeRead>> create(const WorldBundle&,const CharacterConditionWorldRead&,const FamilyContinuationPeopleRead4&,const ProfileControlEligibilitySources&);
 NativeVisitReplyLifeRead(const NativeVisitReplyLifeRead&)=delete;
 NativeVisitReplyLifeRead&operator=(const NativeVisitReplyLifeRead&)=delete;
 Outcome<const VisitReplyReadContext*> context()const;
};
}

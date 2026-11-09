// 跨局发现准入只读真实控制链；卡片查询不创建控制源或投递，历史不以今日玩家重判。
#pragma once
#include "profile_controller_source.hpp"
#include "family_legacy.hpp"
#include "resident/domains/growth/card_acquisition_contract.hpp"
namespace hanmo::resident {
struct ControlContinuationState;
struct GrowthState;struct CareHandoverExecution;
struct AuthoredActorFoundationState;struct ActorFoundationCatalog;class CurrentMomentRead;
struct ProfileControlEligibilitySources {const CommonStore& common;const FamilyState& family;const FamilyCatalog& catalog;const CommonCatalog& shared;const FamilyLegacyState* legacy=nullptr;const AuthoredActorFoundationState* foundation=nullptr;const ActorFoundationCatalog* foundation_catalog=nullptr;const CurrentMomentRead* actual_clock=nullptr;const ControlContinuationState* control_continuation=nullptr;};
struct ProfileControlEligibility {StableId actor_id;ProfileControllerSource source;Integer activation_sequence=0,before_instance_sequence=0;};
struct ProfileAcquisitionEligibility {ProfileControlEligibility discovery_control;bool actor_unlock_eligible=false;};
Outcome<ProfileControlEligibility> read_current_profile_control(const ProfileControlEligibilitySources&);
Outcome<ProfileControlEligibility> read_profile_control_before_fact(const ProfileControlEligibilitySources&,const SourceRef&);
// Actual Growth owner boundary provides the immutable same-M instance cut.
Outcome<ProfileControlEligibility> read_profile_control_before_growth_boundary(const ProfileControlEligibilitySources&,const GrowthState&,const StableId& actual_boundary_id);
// Same original allocator cut, accepted only for a retained genuine Care owner
// boundary. No freely supplied integer or fabricated Common fact.
Outcome<ProfileControlEligibility> read_profile_control_before_care_handover_boundary(
 const ProfileControlEligibilitySources&,const OrderedTable<CareHandoverExecution>&,const StableId& actual_boundary_id);
// delivery_id必须是本世界真实已分配的profile_delivery ID；其序号给同tick不可变历史边界。
Outcome<ProfileControlEligibility> read_profile_control_before_delivery(const ProfileControlEligibilitySources&,const StableId& delivery_id);
// 热写先核当前玩家；冷读沿delivery_id之前的真实链验证。接管旧卡只产生发现，
// ActorStartUnlock仍须证明原条件完成时已受控，不因新接管补授人物解锁。
Outcome<ProfileAcquisitionEligibility> read_current_profile_acquisition_eligibility(const ProfileControlEligibilitySources&,const CardAcquisitionSources&,const CardAcquisitionState&,const ProfileAcquisitionSource&);
Outcome<ProfileAcquisitionEligibility> read_profile_acquisition_delivery_eligibility(const ProfileControlEligibilitySources&,const CardAcquisitionSources&,const CardAcquisitionState&,const ProfileAcquisitionSource&,const StableId& delivery_id);
}

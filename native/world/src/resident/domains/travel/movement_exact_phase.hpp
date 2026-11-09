// 实际S1阶段算法；组合根仅提供当前WorldBundle的具名借用，不能替换专业writer。
#pragma once
#include "movement_action_time.hpp"
#include "cargo_handling_phase.hpp"
#include "patrol_checkpoint.hpp"
#include "../actor_foundation/travel_condition.hpp"
#include "../../kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;class GrowthCapabilityRevisionWrite;
struct MotionExactAuthorSelection {Integer travel_schema=0,group_schema=0,manifest_binding_schema=0,cargo_handling_schema=0,patrol_checkpoint_schema=0,private_transport_funding_schema=0;};
// 原S3/S2消费真正已截断Contact源，返回它实际安装的业务结果；不得再移动/重收费。
class MotionContactBusinessRead {
public:
    virtual ~MotionContactBusinessRead()=default;
    virtual Status validate_actual_contact_installation(const ExactMovementContact&,const MomentOwnerSource&)const=0;
};
class MotionContactBusinessSources:public MotionContactBusinessRead {public:virtual Outcome<MomentOwnerSource> install_actual_contact_in_candidate(const ExactMovementContact&,ExactPhaseWriteContext&)=0;};
struct MotionExactReadBinding {
    const CommonStore& common;const TravelState& travel;const MovementExactState& movement;
    const SpatialCarrierState& carriers;const GroupMovementExecutionState& group_executions;const EconomicTransportExpenseState& payments;
    const TravelCatalog& travel_catalog;const TravelServices& travel_services;const GroupMovementContext* group_context;const AuthoredActorFoundationState& foundation;const ActorFoundationCatalog& actor_catalog;
    const MotionContactBusinessRead& contacts;MotionExactAuthorSelection author;
    const ActionElapsedStore* elapsed=nullptr;const CargoHandlingReadContext* cargo_context=nullptr;
    const PatrolCheckpointOriginalRead* patrol=nullptr;
    const PersonalMovementFundingRead* personal_funding=nullptr;
    const PersonalMovementPartyCostHistoryRead* personal_party_history=nullptr;
};
struct MotionExactWriteBinding {
    CommonStore& common;TravelState& travel;MovementExactState& movement;SpatialCarrierState& carriers;GroupMovementExecutionState& group_executions;
    LocalEconomyState& economy;EconomicTransportExpenseState& payments;const TravelCatalog& travel_catalog;const TravelServices& travel_services;const CurrentMomentRead& current_moment;const GroupMovementContext* group_context;
    AuthoredActorFoundationState& foundation;const ActorFoundationCatalog& actor_catalog;MotionContactBusinessSources& contacts;
    std::function<Status(const StableId&)> original_release_action;
    ActionElapsedStore* elapsed=nullptr;const CargoHandlingWriteContext* cargo_context=nullptr;
    const PatrolCheckpointOriginalRead* patrol=nullptr;
    // Same-candidate owning consumer. It observes actual immutable head changes
    // before the body's travel-exertion effect; no absent/empty handler fallback.
    std::function<Status(ExactPhaseWriteContext&)> consume_actual_spatial_inputs_in_candidate;
    // Borrowed actual-phase original body sources and selected capability owner.
    const CharacterConditionSources* condition_sources=nullptr;
    GrowthCapabilityRevisionWrite* capability_changes=nullptr;
    const PersonalMovementFundingRead* personal_funding=nullptr;
};
// 每次回调仅从参数world产生本栈owning lease，编译注册不持任何候选/Services引用。
class MotionExactReadLease {public:virtual ~MotionExactReadLease()=default;virtual MotionExactReadBinding bindings()const=0;};
class MotionExactWriteLease {public:virtual ~MotionExactWriteLease()=default;virtual MotionExactWriteBinding bindings()=0;};
struct MotionExactWorldFactory {
    Outcome<std::shared_ptr<const MotionExactReadLease>> (*read)(const WorldBundle&)=nullptr;
    Outcome<std::shared_ptr<MotionExactWriteLease>> (*write)(WorldBundle&)=nullptr;
};
MotionExactWorldFactory native_motion_exact_world_factory() noexcept;
// 原 registration 与有限陆路组合共用同一真实借源验证正文。
Status validate_original_motion_phase_author_selection(const WorldBundle&,MotionExactWorldFactory);
Status validate_original_motion_phase_world_sources(const WorldBundle&,const WorldMoment&,MotionExactWorldFactory,ExactArithmeticBudget={});
ExactPhaseRegistration make_motion_exact_phase_registration(MotionExactWorldFactory,ExactArithmeticBudget budget={});
// 紧跟原begin_travel成功后安装，无自由caller轨迹或第二Action。
Outcome<StableId> install_exact_personal_movement_after_begin_in_candidate(CommonStore&,TravelState&,MovementExactState&,const TravelCatalog&,const TravelServices&,const StableId& action_id,const WorldMoment&,ExactArithmeticBudget budget={});
Status validate_motion_exact_author_selection(const MotionExactReadBinding&);
Status validate_motion_exact_sources(const MotionExactReadBinding&,const WorldMoment&,ExactArithmeticBudget budget={});
Outcome<std::vector<ExactEventPrediction>> predict_motion_exact_events(const MotionExactReadBinding&,const WorldMoment&,const WorldMoment&,ExactArithmeticBudget budget={});
Status adopt_motion_exact_interval_from(MotionExactWriteBinding&,const MomentSpan&,ExactPhaseWriteContext&,ExactArithmeticBudget budget={});
Status provide_motion_exact_interval(MotionExactWriteBinding&,const MomentSpan&,ExactPhaseWriteContext&,ExactArithmeticBudget budget={});
Outcome<ExactEventApplied> apply_motion_exact_due(MotionExactWriteBinding&,const ExactEventPrediction&,ExactPhaseWriteContext&,ExactArithmeticBudget budget={});
}

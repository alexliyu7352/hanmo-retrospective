#pragma once
#include "movement_exact_phase.hpp"
#include "../interaction/interaction_exact_encounter.hpp"
namespace hanmo::resident {
struct FieldExactContactReadContext;
struct FieldBattleCatalog;
class NativePersonalMotionContactRead final:public MotionContactBusinessRead {
 const CommonStore& common_;const MovementExactState& movement_;const InteractionState& interaction_;ExactArithmeticBudget budget_;
public:
 NativePersonalMotionContactRead(const CommonStore& c,const MovementExactState& m,const InteractionState& i,ExactArithmeticBudget b={}):common_(c),movement_(m),interaction_(i),budget_(b){}
 Status validate_actual_contact_installation(const ExactMovementContact&,const MomentOwnerSource&)const override;
};
// 真个人Encounter适配；group由S2具名consumer接，不把carrier当Actor。
class NativePersonalMotionContactSources final:public MotionContactBusinessSources {
    CommonStore& common_;TravelState& travel_;MovementExactState& movement_;InteractionState& interaction_;
    const TravelCatalog& catalog_;const TravelServices& services_;const StableId& player_;ExactArithmeticBudget budget_;std::function<Status(const StableId&)> release_action_;
public:
    NativePersonalMotionContactSources(CommonStore& c,TravelState& t,MovementExactState& m,InteractionState& i,const TravelCatalog& catalog,const TravelServices& services,const StableId& player,ExactArithmeticBudget b={},std::function<Status(const StableId&)> release={}):common_(c),travel_(t),movement_(m),interaction_(i),catalog_(catalog),services_(services),player_(player),budget_(b),release_action_(std::move(release)){}
    Outcome<MomentOwnerSource> install_actual_contact_in_candidate(const ExactMovementContact&,ExactPhaseWriteContext&)override;
    Status validate_actual_contact_installation(const ExactMovementContact&,const MomentOwnerSource&)const override;
};
// 原S3真实EncounterLeave/非交互resolved之后，同M接回余程；新unit未走正片不收费。
Status resume_exact_movement_after_encounter_in_candidate(CommonStore&,TravelState&,MovementExactState&,const InteractionState&,const TravelCatalog&,const TravelServices&,const StableId& encounter_id,const WorldMoment&,ExactArithmeticBudget budget={});
Status validate_movement_encounter_continuations(const CommonStore&,const MovementExactState&,const InteractionState&,ExactArithmeticBudget budget={}, const FieldExactContactReadContext* field=nullptr, const FieldBattleCatalog* field_catalog=nullptr);
}

#pragma once
#include "office_exact.hpp"
#include "formation_exact.hpp"
#include "resupply_exact.hpp"
#include "return_exact.hpp"
#include "military_return_source.hpp"
#include "military_formation_source.hpp"
#include "military_exact_sources.hpp"
namespace hanmo::resident {
// Explicit selected Time2 cold entry. All references belong to the same decoded
// or live candidate. Contexts are source-local and must not call this validator.
// Their absence is allowed only when no row/active Action of that kind exists.
struct ExactMilitaryStateValidationContext {
 const MilitaryActionTimeState& action_time;
 const ActionElapsedStore& elapsed;
 const MilitaryFormationSourceRead& formations;
 const CurrentMomentRead& clock;
 const ExactTrainingPeriodRead& periods;
 const ExactOfficeReadContext* offices=nullptr;
 const ExactFormationReadContext* formation_actions=nullptr;
 const ExactResupplyReadContext* resupply=nullptr;
 const ExactTrainingReadContext* training=nullptr;
 const MilitaryExactState* dynamic_military=nullptr;
 ExactArithmeticBudget budget{};
 const ExactReturnReadContext* return_actions=nullptr;
 const MilitaryReturnSourceRead* returns=nullptr;
 // Explicit Field1 /1 Opening supply; not inferred from missing dynamic rows.
 bool field_supply_source1=false;
};
Status validate_military_state(const CommonStore&,const MilitaryState&,
 const MilitaryCatalog&,const MilitaryAccess&,const ExactMilitaryStateValidationContext&);
}

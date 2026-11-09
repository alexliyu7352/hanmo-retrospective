#pragma once
#include "military_tactical_intent_model.hpp"
#include "dynamic_action.hpp"
#include "resident/kernel/common_command_sources.hpp"
namespace hanmo::resident {
class MilitaryAcceptedIntentVisitor {
public:
 virtual ~MilitaryAcceptedIntentVisitor()=default;
 virtual Status accepted_intent(const MilitaryAcceptedIntent3& actual_borrowed_row)=0;
};
// The real missions owner supplies these reads, never a parallel row cache.
// Every callback is source-local and may not call this full accepted-intent
// validator, DynamicAction validation, or the whole session validator.
class MilitaryTacticalIntentOriginalRead {
public:
 virtual ~MilitaryTacticalIntentOriginalRead()=default;
 virtual Integer selected_intent_schema()const noexcept=0;
 virtual bool bound_to(const CommonStore&,const MilitaryExactState&)const noexcept=0;
 virtual Outcome<const MilitaryAcceptedIntent3*> read_original_accepted_intent(const SourceRef&)const=0;
 virtual Status visit_original_accepted_intents(MilitaryAcceptedIntentVisitor&)const=0;
 // Original containing mission, formation/member/control history, upper scope,
 // authored timing/order and actual acceptance cut. Current control is not a
 // historical proof. Each source schema is fixed by its actual original owner.
 virtual Status validate_original_intent_scope(const MilitaryAcceptedIntent3&)const=0;
 // The original sealed command payload must bind this exact accepted row and
 // controller/epoch/mission at its actual M. A successful unrelated command
 // (including a legacy strategic order) is not sufficient.
 virtual Status validate_original_direct_intent_command(const MilitaryAcceptedIntent3&,
     const CommonCommandSourceView&)const=0;
 // Original trigger/knowledge/policy/control/RNG proof only. The core below
 // verifies selected_intent_refs in BOTH directions without recursive reads.
 virtual Status validate_original_decision_inputs(const DynamicSessionStateSource2&)const=0;
 // Full original order -> real delivery -> actual Adopted response, matching
 // this mission/formation and finite scope. Missing policy/source is Required.
 virtual Status validate_original_delivered_intent(const MilitaryAcceptedIntent3&)const=0;
};
struct MilitaryAcceptedIntentReadContext {
 const CommonStore& common;const MilitaryExactState& exact;
 const MilitaryTacticalIntentOriginalRead& originals;const CurrentMomentRead& clock;
 ExactArithmeticBudget budget={};
};
MomentOwnerSource military_accepted_intent_source3(const StableId&);
Outcome<const MilitaryAcceptedIntent3*> read_verified_military_accepted_intent3(
 const MilitaryAcceptedIntentReadContext&,const MomentOwnerSource&);
// Real finite projection for DynamicMilitaryTacticalSources::read_accepted_intent.
// It copies no mission, control state or elapsed progress into the action.
Outcome<DynamicMilitaryAcceptedIntentRead> read_military_accepted_action_intent3(
 const MilitaryAcceptedIntentReadContext&,const SourceRef&);
Status validate_military_accepted_intent_sources3(const MilitaryAcceptedIntentReadContext&);
// No producer is supplied by this source reader: the existing mission owning
// row and actual Direct/Decision/DeliveredOrder acceptance writer are required.
}

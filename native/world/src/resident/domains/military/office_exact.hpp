#pragma once
#include "training_exact_model.hpp"
#include "resident/kernel/action_time_validation.hpp"
#include "resident/domains/interaction/interaction_role_history.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/kernel/source_retention.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct PreparedCatalog;struct OrganizationState;struct FamilyState;
struct CharacterConditionSources;
class CharacterAvailabilityProvenanceSink;
// Original history only. This provider must read the accepted interaction,
// participant/placement and actual Office changes; current holder is not a
// historical acceptance or a full-span participation proof. These three reads
// are source-local: they must not call the S0 elapsed validator or re-enter this
// owner. Stable S0 coverage and release checks are performed by the caller.
class ExactOfficeOriginalRead {
public:
 virtual ~ExactOfficeOriginalRead()=default;
 virtual bool bound_to(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&)const noexcept=0;
 virtual Status validate_office_acceptance(const DynamicMilitaryActionFact2&)const=0;
 virtual Status validate_office_actual_service(const DynamicMilitaryActionFact2&,const MomentSpan&)const=0;
 virtual Status validate_office_completed_result(const DynamicMilitaryActionFact2&)const=0;
};
struct ExactOfficeReadContext {
 const MilitaryCatalog& catalog;const MilitaryAccess& access;const TimeScaleRef& scale;
 const ExactOfficeOriginalRead& original;const CurrentMomentRead& clock;
 const PreparedCatalog& prepared;const OrganizationState& organization;const FamilyState& family;
 const CharacterConditionSources& body;
 ExactArithmeticBudget budget{};
};
// Finite source roots only. This is not the three-method Original provider.
// The selected caller owns every borrowed root and validates its World binding.
struct ExactOfficeSourceReadContext {
 const CommonStore& common;const MilitaryState& military;
 const MilitaryActionTimeState& actions;const ActionElapsedStore& elapsed;
 const MilitaryCatalog& catalog;const MilitaryAccess& access;const TimeScaleRef& scale;
 const CurrentMomentRead& clock;
 const PreparedCatalog& prepared;const OrganizationState& organization;const FamilyState& family;
 const CharacterConditionSources& body;
 ExactArithmeticBudget budget{};Integer selected_action_time=0;
};
struct ExactOfficeWriteContext {ExactOfficeReadContext read;std::function<Status(const StableId&)> release_action;};
ActionTimeRegistration exact_office_action_registration();
class NativeOfficeActionTimeRead final:public ActionTimeOriginalSourceRead {
 const CommonStore& common_;const MilitaryState& military_;const MilitaryActionTimeState& state_;
 const ActionElapsedStore& elapsed_;ExactOfficeReadContext context_;
public:
 NativeOfficeActionTimeRead(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& e,ExactOfficeReadContext x):common_(c),military_(m),state_(s),elapsed_(e),context_(x){}
 Status enumerate_action_time_owners(const ActionTimeRegistration&,ActionTimeOwnerIdSink&)const override;
 Outcome<ActionTimeOwnerProof> read_action_time_owner(const ActionTimeRegistration&,const StableId&)const override;
 Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource&)const override;
 Status validate_fixed_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&)const override;
 Status validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const override;
 Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<OriginalActionRemaining> read_original_action_remaining(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<StableId> read_gate_result_id(const SharedWorkGateResult&)const override;
 Outcome<SharedWorkGateResult> read_original_gate_result(const StableId&)const override;
 Status validate_original_gate_result(const SharedWorkGateResult&)const override;
};
Outcome<StableId> start_exact_office_action_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactOfficeWriteContext&,const ActionCoreView&,const OfficeAccept&,const MilitaryQuote&);
Status advance_exact_office_action_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactOfficeWriteContext&,const StableId&,const MomentSpan&);
Outcome<MilitaryEffect> complete_exact_office_action_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactOfficeWriteContext&,const StableId&);
Status validate_exact_office_sources(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactOfficeReadContext&,bool allow_current_completion_candidate=false);
Outcome<const DynamicMilitaryActionFact2*> read_verified_exact_office_result(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactOfficeReadContext&,const SourceRef&,Integer schema,CharacterAvailabilityProvenanceSink* retained_body=nullptr);
// Actual application identity and its original exact result/work binding only.
// This does not re-enter acceptance/service, nor replace whole stable closure.
Outcome<const DynamicMilitaryActionFact2*> read_exact_office_completion_application(
 const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,
 const ExactOfficeReadContext&,const SourceRef&,Integer source_schema,
 CommonActualFactPrefixSink* retained_prefix=nullptr,
 std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* start_role=nullptr,
 std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* successful_completion_role=nullptr);
Outcome<const DynamicMilitaryActionFact2*> read_exact_office_completion_application(
 const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,
 const ExactOfficeSourceReadContext&,const SourceRef&,Integer source_schema,
 CommonActualFactPrefixSink* retained_prefix=nullptr,
 std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* start_role=nullptr,
 std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* successful_completion_role=nullptr);
// Complete original result validation, reached by an actual application head.
// Sink output, including checked suffix dependencies, is valid only after this entire read succeeds.
// An absent sink is validation only, not a retained-provenance claim.
Outcome<const DynamicMilitaryActionFact2*> read_verified_exact_office_application(
 const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,
 const ExactOfficeReadContext&,const SourceRef&,Integer source_schema,
 CommonActualFactPrefixSink* retained_prefix=nullptr,
 std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* start_role=nullptr,
 std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* successful_completion_role=nullptr,
 CharacterAvailabilityProvenanceSink* retained_body=nullptr);

// Finite actual source slots for this original completion application only.
// Same borrowed traversal feeds enumeration and exact membership validation.
Status collect_exact_office_completion_pins(const CommonStore&,const MilitaryState&,
 const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactOfficeReadContext&,SourcePinSink&);
Status validate_exact_office_completion_pin(const CommonStore&,const MilitaryState&,
 const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactOfficeReadContext&,const SourcePin&);
std::vector<RequiredSourceConsumerSlot> exact_office_completion_consumer_slots();
Outcome<bool> exact_office_completion_source_may_release(const CommonStore&,const MilitaryState&,
 const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactOfficeReadContext&,const SourceRef&,Integer source_schema);

}

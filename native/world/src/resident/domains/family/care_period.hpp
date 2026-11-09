// P CHR/CARE and v1c 3: original completed Calendar care, not another clock.
#pragma once
#include "care_coverage.hpp"
#include "resident/domains/local_economy/economic_calendar.hpp"
namespace hanmo::resident {
struct CareCalendarWindow {Integer from_tick=0,to_tick=0;};
enum class CarePeriodOutcome {completed};
// The dictionary key is the actual care_period_ref; it is not copied into a
// second mutable ID field. Place/activity/food retain the actual owner source.
struct CarePeriod {
 StableId child,caregiver;MomentOwnerSource place;CareCalendarWindow period;
 MomentOwnerSource activity_ref,food_support_ref;
 CarePeriodOutcome outcome=CarePeriodOutcome::completed;
};
struct OriginalCareActionParticipation {
 MomentOwnerSource original_action_ref,provided_period_ref;
};
struct CompatibleFamilyActivityParticipation {
 MomentOwnerSource family_activity_ref;ActorFoundationRuleRef compatibility_rule_ref;
 MomentOwnerSource actual_participation_ref;
};
using CarePeriodParticipation=std::variant<OriginalCareActionParticipation,CompatibleFamilyActivityParticipation>;
struct CarePeriodSource {
 StableId care_period_ref,binding_ref;MomentOwnerSource calendar_period_ref;
 CarePeriodParticipation participation;MomentOwnerSource actual_complete_source_ref;
 std::vector<MomentOwnerSource> provision_result_refs;
 std::vector<ConditionReceiptRefV2> condition_result_refs;
};
struct CarePeriodReadTables {const OrderedTable<CarePeriod>& periods;const OrderedTable<CarePeriodSource>& sources;};
struct CarePeriodWriteTables {OrderedTable<CarePeriod>& periods;OrderedTable<CarePeriodSource>& sources;
 CarePeriodReadTables read()const{return {periods,sources};}
};
class CarePeriodExpectedSourceVisitor {public:virtual ~CarePeriodExpectedSourceVisitor()=default;
 virtual Status actual_completed_period(const StableId& binding,
  const MomentOwnerSource& calendar,const MomentOwnerSource& original_completion)=0;
};
// The concrete original service owner verifies its actual opening commitment,
// full-span participant/space/compatibility, accepted rights, unique S5 mouth,
// and original Condition effects. Even an empty effect list needs that proof.
// This interface is not a generic service-success implementation. Its source
// reads must never call this CarePeriod consumer or use its output as evidence.
class CarePeriodOriginalRead {
public:virtual ~CarePeriodOriginalRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Status validate_original_completed_care_period(const CareBinding&,
  const AcceptedCaregiver&,const CarePeriod&,const CarePeriodSource&,
  const EconomicPeriodOpening&,const SourceRef& actual_care_period)const=0;
 // Reverse coverage over genuine complete original services adopted for Care;
 // partial service and an unaccepted relative's activity are not candidates.
 virtual Status visit_original_completed_care_periods(CarePeriodExpectedSourceVisitor&)const=0;
};
struct CareOriginalActionPeriodSources;
struct CarePeriodSources {
 const CareCoverageSources& coverage;CarePeriodReadTables tables;
 const EconomicCalendarReadContext& calendar;const CarePeriodOriginalRead& originals;
 const CareOriginalActionPeriodSources* original_actions=nullptr;
};
SourceRef care_period_source(const StableId&);
Outcome<Integer> care_period_source_schema(const SourceRef&);
Status validate_care_period_shape(const CarePeriod&);
Status validate_care_period_source_shape(const CarePeriodSource&);
bool care_period_equal(const CarePeriod&,const CarePeriod&);
bool care_period_inputs_equal(const CarePeriodSource&,const CarePeriodSource&);
struct CarePeriodReadView {const CarePeriod* period=nullptr;const CarePeriodSource* source=nullptr;const EconomicPeriodOpening* opening=nullptr;};
Outcome<CarePeriodReadView> read_care_period(const CarePeriodSources&,const SourceRef&,Integer source_schema);
Status validate_care_period_sources(const CarePeriodSources&);
struct CarePeriodApplication {SourceRef period_ref;bool already_provided=false;};
// Called only after the original service, S5 feeding and its original effects
// succeeded. Input care_period_ref must be empty; the sole owner allocates it.
// This adds no time, food, fatigue or award. Any failure discards the complete
// private candidate, including the prior original service writes.
Outcome<CarePeriodApplication> provide_care_period_in_candidate(CommonStore&,
 CarePeriodWriteTables,const CarePeriodSources&,CarePeriod,CarePeriodSource);
}

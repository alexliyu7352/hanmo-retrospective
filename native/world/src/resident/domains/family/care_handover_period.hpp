// FH5.3B actual complete-N period producer. This never advances Fixed credit.
#pragma once
#include "care_handover_worker_read.hpp"
namespace hanmo::resident {
// Original authority/participation/facility and accepted cost owners. The
// concrete lease must verify lawful empty costs as well; no default +8/free leg.
class CareHandoverPeriodOriginalRead {
public:virtual ~CareHandoverPeriodOriginalRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Status validate_period_inputs(const CareHandoverExecution&,
  const AcceptedCaregiver&,const FixedServiceWorkPeriodRead2&)const=0;
 virtual Status validate_period_effects(const CareHandoverExecution&,
  const AcceptedCaregiver&,const CareHandoverWorkPeriod2&)const=0;
};
class CareHandoverPeriodOriginalWrite:public CareHandoverPeriodOriginalRead {
public:
 // Called once, after the actual source row is inserted, before publication.
 // This is the original accepted Care cost rule's real writer; it returns only
 // receipts it actually applied and must immediately adopt each capability
 // change at the actual write, not sweep a completed group afterward.
 virtual Outcome<std::vector<ConditionReceiptRefV2>> apply_period_effects_in_candidate(
  CommonStore&,const CareHandoverExecution&,const AcceptedCaregiver&,
  const CareHandoverWorkPeriod2&)=0;
};
struct CareHandoverPeriodApplication {SourceRef period_source;bool already_applied=false;};
Outcome<CareHandoverPeriodApplication> provide_care_handover_period_in_candidate(
 CommonStore&,OrderedTable<CareHandoverExecution>&,const CareHandoverSourceContext&,
 const StableId& actual_handover,Integer actual_period_ordinal,
 CareHandoverPeriodOriginalWrite&);
Status validate_care_handover_period_sources(
 const CareHandoverSourceContext&,const CareHandoverPeriodOriginalRead&);
}

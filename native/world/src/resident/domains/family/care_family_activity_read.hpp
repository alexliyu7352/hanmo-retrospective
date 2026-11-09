#pragma once
#include "care_family_activity.hpp"
#include "care_handover_command_source.hpp"
#include "care_coverage.hpp"
namespace hanmo::resident {
// Original guardianship, accepted duties, provision and fees are independent
// source owners. Historical checks take the real Common adoption cut. They
// must not use this activity or CoverageSatisfied as authority for themselves.
class CareFamilyActivityAuthorityRead {
public:virtual ~CareFamilyActivityAuthorityRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Status validate_current_activity_authority(const AcceptedCaregiver&,
  const WorldMoment&)const=0;
 virtual Status validate_activity_authority_before_source(const AcceptedCaregiver&,
  const SourceRef& actual_activity_start,const WorldMoment&)const=0;
 // Real original participation and accepted fee results at the completed
 // handover. These references must be proved independently of the new row.
 virtual Status validate_actual_handover_inputs(const CareHandoverExecution&,
  const AcceptedCaregiver&,const WorldMoment&,
  const std::vector<MomentOwnerSource>& participation,
  const std::vector<MomentOwnerSource>& fee_results)const=0;
 // The ordinary occupation owner or original StopCare/authority writer owns
 // these actual causes. There is no default "idle" or "still accepted" proof.
 virtual Status validate_original_activity_end(const CareFamilyActivity&,
  const CareFamilyActivityEnd&,const SourceRef& actual_end)const=0;
 virtual Status validate_original_activity_continuation(const CareFamilyActivity&,
  const AcceptedCaregiver&,const MomentSpan&,const SourceRef* actual_end)const=0;
};
struct CareFamilyActivitySources {
 const CareHandoverCommandSourceContext& commands;
 const CareCoverageSources& coverage;
 const OrderedTable<CareFamilyActivity>& activities;
 const CareFamilyActivityAuthorityRead& authority;
};
struct CareFamilyActivityRead {
 const CareFamilyActivity* activity=nullptr;
 const AcceptedCaregiver* accepted=nullptr;
 const CareHandoverExecution* execution=nullptr;
 const CareTemporalEvent* completed=nullptr;
};
struct CareFamilyActivityWriteBinding {
 OrderedTable<CareFamilyActivity>& activities;
 const CareFamilyActivitySources& sources;
};
// Stable, borrowed original source. The actual completion must exist; private
// start-before-Satisfied uses only the non-exportable proof in the writer.
Outcome<CareFamilyActivityRead> read_care_family_activity(
 const CareFamilyActivitySources&,const MomentOwnerSource& actual_start);
// Only the independently established original start. Consumers of an actual
// later end cause use this prefix to avoid making that cause validate itself.
// Stable publication still calls the complete read/closure above and below.
Outcome<CareFamilyActivityRead> read_care_family_activity_start_provenance(
 const CareFamilyActivitySources&,const MomentOwnerSource& actual_start);
// Both source kinds borrow their actual owner row and original occurrence.
// This does not replace the publication closure below or invent a Common cut.
struct CareFamilyActivityActualSourceRead {
 const CareFamilyActivity* activity=nullptr;
 const CareFamilyActivityEnd* actual_end=nullptr;
 const WorldMoment* occurred_at=nullptr;
};
Outcome<CareFamilyActivityActualSourceRead> read_care_family_activity_source(
 const CareFamilyActivitySources&,const SourceRef&,Integer source_schema);
Status validate_care_family_activity_sources(const CareFamilyActivitySources&);
// These are called inside the original loss/closure writers, after their
// real cause has occurred and before Lost/Closed is appended. Only the four
// already implemented Death/window/spatial and actual Growth-start branches
// can produce an end. Other occupation owners still need their real sources.
Status end_care_family_activity_for_loss_in_candidate(CommonStore&,
 CareFamilyActivityWriteBinding&,const CareBinding&,const CareLossCauseV2&);
Status end_care_family_activity_for_subject_death_in_candidate(CommonStore&,
 CareFamilyActivityWriteBinding&,const CareBinding&,const DeathRef&);
class NativeCareFamilyActivityRead final:public CareCoverageOriginalRead {
 const CareFamilyActivitySources& sources_;
public:explicit NativeCareFamilyActivityRead(const CareFamilyActivitySources& s):sources_(s){}
 bool bound_to(const CommonStore&)const noexcept override;
 const CareFamilyActivitySources* family_activity_sources()const noexcept override{return &sources_;}
 Status validate_original_completed_handover(const CareHandoverExecution&,
  const AcceptedCaregiver&,const CareBinding&,const CareTemporalEvent&)const override;
 Status validate_current_care_participation(const CareBinding&,
  const AcceptedCaregiver&,const WorldMoment&)const override;
};
}

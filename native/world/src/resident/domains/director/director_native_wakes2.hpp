#pragma once
#include "director_subject2.hpp"
#include "resident/domains/tasks/work_execution/work_supervision_sources.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
namespace hanmo::resident {
using DirectorOriginalWakeVisitor2=std::function<Status(const SubjectWake2&,const WorldMoment&)>;
struct DirectorWorkWakeRead2 {
 const CommonStore& common;const TaskState& tasks;const WorkTaskRead& definitions;
 const WorkOriginalRead& original;const SupervisionSourceContext& supervision;
 const WorldMoment& actual;
};
Status visit_director_work_wakes2(const DirectorWorkWakeRead2&,const DirectorOriginalWakeVisitor2&);
Status validate_director_work_wake2(const DirectorWorkWakeRead2&,const SubjectWake2&);
Status visit_director_capability_wakes2(const CharacterConditionSources&,const DirectorOriginalWakeVisitor2&);
Status validate_director_capability_wake2(const CharacterConditionSources&,const SubjectWake2&);
// Uses the existing hot index to distinguish newly applied real sources. Cold
// rebuild remains a read-only restoration of the saved wake_reasons, not a way
// to invent new historical pending events.
Status collect_director_owner_wakes2(DirectorState&,const DirectorSubjectOwnerRead&,const WorldMoment&);
}

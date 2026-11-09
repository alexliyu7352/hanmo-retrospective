#pragma once
#include "director_subject2.hpp"
#include "resident/domains/organization/authority_change_read.hpp"
namespace hanmo::resident {
// Borrow one unchanged candidate and its actual phase stamp. Authority changes wake
// a review; they do not deliver a report, sign consent, or grant business authority.
struct DirectorAuthorityWakeRead2 {
 const CommonStore& common;
 const OrganizationState& organization;
 const OrganizationCatalog& catalog;
 const ReadStamp& stamp;
 const OfficeTransitionSourceRead& transitions;
 const VerifiedDeathRead& deaths;
 const OrganizationAuthorityClock& clock;
 const WorldMoment& actual;
};
using DirectorAuthorityWakeVisitor2=std::function<Status(const SubjectWake2&,const WorldMoment&)>;
Status visit_director_authority_source_wakes2(const DirectorAuthorityWakeRead2&,const SourceRef&,Integer schema,const DirectorAuthorityWakeVisitor2&);
Status visit_director_authority_wakes2(const DirectorAuthorityWakeRead2&,const DirectorAuthorityWakeVisitor2&);
Status validate_director_authority_wake2(const DirectorAuthorityWakeRead2&,const SubjectWake2&);
Status collect_director_authority_wakes2(const DirectorAuthorityWakeRead2&,DirectorState&);
}

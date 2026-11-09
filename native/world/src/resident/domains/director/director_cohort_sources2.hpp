// S5 owns every plan, population selection, authority and purchase. This is a
// pure Director projection of already accepted plans, never a plan producer.
#pragma once
#include "director_cohort_purchase2.hpp"
#include "director_knowledge_wake2.hpp"
namespace hanmo::resident {
struct DirectorCohortSourceContext2 {
 const DirectorState& director;const DirectorCatalog& catalog;
 const HouseholdPurchaseContext& purchase;const SocialState& social;
 const ExactResultTimeRead& results;const ExactResultReadStamp& stamp;
};
class NativeDirectorCohortSources2 {
 const DirectorCohortSourceContext2& x_;
public:
 explicit NativeDirectorCohortSources2(const DirectorCohortSourceContext2& x):x_(x){}
 bool bound_to(const CommonStore&,const DirectorState&)const noexcept;
 Outcome<std::optional<SubjectAdmissionRead2>> admission(const SubjectRef&)const;
 Outcome<CohortKnownSubjectView> known(const SubjectRef&)const;
 Status visit_admissions(const std::function<Status(const SubjectAdmissionRead2&)>&)const;
};
}

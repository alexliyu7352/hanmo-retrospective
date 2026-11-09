#pragma once
#include "dynamic_result_time2.hpp"
#include "resident/domains/director/director_authority_wake2.hpp"
namespace hanmo::resident {
// Only the actual Office/Grant successor is handled here. Other S6 sources must
// be bound to their own original time reader, never coerced to a Calendar tick.
bool native_organization_authority_result_kind(const FactRecord&);
class NativeOrganizationAuthorityResultRead final:public ExactResultOwnerRead {
 DirectorAuthorityWakeRead2 source_;
public:
 explicit NativeOrganizationAuthorityResultRead(DirectorAuthorityWakeRead2 source):source_(source){}
 Outcome<ExactOwnerFactRead> read_selected_fact(const FactRecord&)const override;
 Outcome<WorldMoment> read_selected_source_moment(const MomentOwnerSource&)const override;
};
}

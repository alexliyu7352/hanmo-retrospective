#pragma once
#include "dynamic_result_time2.hpp"
#include "resident/domains/politics/politics_time2.hpp"
namespace hanmo::resident {
struct PoliticsResultProjectionAccess {
 const CommonStore& common;const PoliticsState& politics;
 const OrganizationRuntime& organization_runtime;const OrganizationAuthorityClock& clock;
};
bool native_politics_time2_result_kind(const FactRecord&);
class NativePoliticsTime2ResultRead final:public ExactResultOwnerRead {
 PoliticsResultProjectionAccess access_;
public:explicit NativePoliticsTime2ResultRead(PoliticsResultProjectionAccess x):access_(x){}
 Outcome<ExactOwnerFactRead> read_selected_fact(const FactRecord&)const override;
 Outcome<WorldMoment> read_selected_source_moment(const MomentOwnerSource&)const override;
};
}

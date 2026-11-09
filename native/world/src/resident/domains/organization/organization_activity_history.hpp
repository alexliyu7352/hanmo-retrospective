// S5真实Opening的事实切点读口；同Moment开后新活动不能靠floor tick混入当期。
#pragma once
#include "organization_model.hpp"
namespace hanmo::resident {
class OrganizationActivityOpeningRead {public:virtual ~OrganizationActivityOpeningRead()=default;virtual Status read_organization_activity_at_opening(const StableId& activity_id,const SourceRef& actual_opening_source,const WorldMoment& opening_at)const=0;};
// 完整业务来源时间由原owner提供，旧Calendar来源必须显式绑定原整数边界。
class OrganizationActivitySourceMomentRead {public:virtual ~OrganizationActivitySourceMomentRead()=default;virtual Outcome<WorldMoment> read_organization_activity_source_moment(const OrganizationFact&)const=0;};
class NativeOrganizationActivityOpeningRead final : public OrganizationActivityOpeningRead {
public:
 NativeOrganizationActivityOpeningRead(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,const OrganizationActivitySourceMomentRead& moments):common_(c),state_(s),catalog_(catalog),moments_(moments){}
 Status read_organization_activity_at_opening(const StableId&,const SourceRef&,const WorldMoment&)const override;
private:const CommonStore& common_;const OrganizationState& state_;const OrganizationCatalog& catalog_;const OrganizationActivitySourceMomentRead& moments_;
};
}

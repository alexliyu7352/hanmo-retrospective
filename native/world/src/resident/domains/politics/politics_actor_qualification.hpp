// 原独立公职成年门，消费S4真实年龄基线；不反造生日或取消旧任职。
#pragma once
#include "politics_organization_runtime.hpp"
#include "resident/domains/family/life_age.hpp"
namespace hanmo::resident {
class NativePoliticalActorQualificationRead final:public PoliticalActorQualificationRead {
 const LifeAgeSources& ages_;const OrganizationAuthorityClock& clock_;
public:
 NativePoliticalActorQualificationRead(const LifeAgeSources& ages,const OrganizationAuthorityClock& clock):ages_(ages),clock_(clock){}
 Status validate_independent_office_age(const StableId&,const WorldMoment&)const override;
};
}

#include "politics_actor_qualification.hpp"
namespace hanmo::resident {
Status NativePoliticalActorQualificationRead::validate_independent_office_age(const StableId& actor,const WorldMoment& at)const{
 auto now=read_organization_authority_now(ages_.common,&clock_);if(!now.ok)return {false,{},now.error};
 auto same=compare_world_moments(now.value,at);if(!same.ok)return {false,{},same.error};
 if(same.value)return common_failure("PoliticalQualificationCurrentMomentMismatch",actor);
 auto age=read_life_age_basis(ages_,actor,at);if(!age.ok)return {false,{},age.error};
 // 03§3.15及原growth_independent_office_age唯一规则：成年为18整岁。
 // DeclaredAgeAtStart/真实Birth由原年龄reader分别解释，不用Common.birth_tick兜底。
 if(age.value.age_years<18)return common_failure("OfficeRequiresAdult",actor);
 return common_success();
}
}

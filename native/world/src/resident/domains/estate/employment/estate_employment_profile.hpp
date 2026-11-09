// 独立雇用1段显式选择新工资来源；旧Estate与LE银行不增加合同或隐式升级。
#pragma once
#include "estate_employment.hpp"
namespace hanmo::resident {
// Estate和Interaction属于原世界必有冷段；动态工资必须有唯一原LE版本标记。
inline Status validate_estate_employment_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules){
 auto count=[&](const StableId& key){return std::count_if(modules.begin(),modules.end(),[&](const auto& x){return x.first==key;});};
 auto has=[&](const StableId& key,const StableId& version){return std::find(modules.begin(),modules.end(),std::make_pair(key,version))!=modules.end();};
 if(!enabled)return count(estate_employment_module)?common_failure("UnexpectedEstateEmploymentProfile"):common_success();
 if(count(estate_employment_module)!=1||!has(estate_employment_module,"1"))return common_failure("EstateEmploymentProfileMismatch");
 if(count("dynamic_world")!=1||!has("dynamic_world","local_society_v1")||count("local_economy")!=1)return common_failure("EstateEmploymentDependenciesRequired");
 if(!has("local_economy","1")&&!has("local_economy","2")&&!has("local_economy","3")&&!has("local_economy","4")&&!has("local_economy","5"))return common_failure("EstateEmploymentDependenciesRequired");
 return common_success();
}
}

// 新税债Need有独立模块身份；旧pair1和Need1银行不因更新而新增必需投影。
#pragma once
#include "fiscal_need_adapter.hpp"
namespace hanmo::resident {
inline constexpr const char* occupation_fiscal_need_module="occupation_fiscal_needs";
// 没有独立金额或预约段，实际Need仍由原Affairs codec保存；此marker选择其严格来源语义。
inline Status validate_occupation_fiscal_need_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules){
 auto count=[&](const StableId& name){return std::count_if(modules.begin(),modules.end(),[&](const auto& x){return x.first==name;});};
 auto has=[&](const StableId& name,const StableId& version){return std::find(modules.begin(),modules.end(),std::make_pair(name,version))!=modules.end();};
 const auto marker=count(occupation_fiscal_need_module);if(!enabled)return marker?common_failure("UnexpectedOccupationFiscalNeedProfile"):common_success();
 if(marker!=1||!has(occupation_fiscal_need_module,"1"))return common_failure("OccupationFiscalNeedProfileMismatch");
 for(const auto* dependency:{"dynamic_world","political_occupation_fiscal","occupation_fiscal_settlement","fiscal_settlement","fiscal_need_policies","affairs"})if(count(dependency)!=1)return common_failure("OccupationFiscalNeedDependenciesRequired",dependency);
 if(!has("dynamic_world","local_society_v1")||!has("political_occupation_fiscal","1")||!has("occupation_fiscal_settlement","1")||!has("fiscal_settlement","1")||!has("fiscal_need_policies","1")||(!has("affairs","3")&&!has("affairs","4")))return common_failure("OccupationFiscalNeedDependenciesRequired");
 return common_success();
}
// 原生作者路径同样严格闭集，不把false、缺version或未知字段当作安全默认。
inline Status decode_occupation_fiscal_need_profile(const BoundaryValue& value){try{
 using namespace boundary;fields(value,{"enabled","version"},{},"occupation_fiscal_needs");
 if(!get_bool(value,"enabled","occupation_fiscal_needs")||get_int(value,"version","occupation_fiscal_needs")!=1)return common_failure("UnsupportedOccupationFiscalNeedProfile");
 return common_success();
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
}

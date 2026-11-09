// 原Family生前换控的当前准备门；不会授予关系、本人同意或转移公职。
#pragma once
#include "family_model.hpp"
namespace hanmo::resident {
// 当前原命令分流的唯一门，World薄适配与旧领域preview共用；不修改旧profile。
inline Status validate_voluntary_family_command_route(const FamilyCatalog& catalog,const StableId& kind){
    if(catalog.voluntary_source_mode==FamilyVoluntarySourceMode::living_paths){
        if(kind=="family_retire")return common_failure("VoluntaryAcceptedPlanRequired");
        if(kind=="family_agree_succession"||kind=="family_cancel_succession"||kind=="family_continue_as")return common_failure("VoluntaryPathActualResultAdapterRequired");
    }else if(catalog.voluntary_source_mode!=FamilyVoluntarySourceMode::authored_roles)return common_failure("UnsupportedFamilyVoluntarySourceMode");
    return common_success();
}

Status validate_voluntary_family_parties_ready(const CommonStore&,const FamilyState&,const EstateState&,
    const FamilyAccess&,const StableId& predecessor,const StableId& successor);
}

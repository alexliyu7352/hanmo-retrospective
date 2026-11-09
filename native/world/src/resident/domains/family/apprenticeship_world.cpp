// 不另存身份、生命、军事占用或课程真值；所有回调只借调用者这份稳定/候选 World。
#include "apprenticeship_world.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
bool apprenticeship_command_kind(const StableId& kind){return kind=="family_apprenticeship_receive"||kind=="family_apprenticeship_reply"||kind=="family_apprenticeship_exit";}
ApprenticeshipIdentityRead WorldBundle::family_apprenticeship_access()const{
    return {family_birth_enabled()?&family_birth:nullptr,family_death_read(),family_bequest_player_read(),family_legacy_playable_read()};
}
Status validate_family_apprenticeship_world(const WorldBundle& w){
    const bool enabled=w.family_apprenticeship_enabled();Integer markers=0;
    for(const auto& module:w.catalog->identity.module_versions)if(module.first=="family_apprenticeship"){++markers;if(module.second!="1")return common_failure("UnsupportedApprenticeshipVersion");}
    if(enabled!=w.family_apprenticeship_seed_complete||markers!=(enabled?1:0)||bool(w.catalog->family_apprenticeship)!=enabled||(enabled&&(!w.dynamic_enabled()||!w.family_life_enabled()||!w.catalog->family->enabled||!w.catalog->growth->cultivation)))return common_failure("ApprenticeshipProfileMismatch");
    if(w.catalog->interaction->common.get()!=w.catalog->common.get())return common_failure("ApprenticeshipCommonCatalogMismatch");
    auto facts=validate_family_apprenticeship(w.common,*w.catalog->interaction,enabled?*w.catalog->family_apprenticeship:ApprenticeshipCatalog{},w.family_apprenticeship,w.family_apprenticeship_access());if(!facts.ok)return {false,{},facts.error};
    auto checked=validate_apprenticeship_result_bindings(w.common,w.family_apprenticeship);if(!checked.ok)return checked;
    auto prefix=[](const StableId& kind){return kind.rfind("family_apprenticeship_",0)==0;};
    for(const auto& old:w.common.state().history)if(prefix(old.kind)&&(!enabled||!apprenticeship_command_kind(old.kind)))return common_failure("ApprenticeshipProfileRequired");
    for(const auto& row:w.common.state().results.rows())if(row.second){
        const auto& result=*row.second;const auto* receipt=result.details?std::get_if<FamilyEffect>(&result.details->data):nullptr;const bool typed=receipt&&receipt->fact_id&&receipt->fact_id->rfind("apprenticeship:",0)==0;
        if(prefix(result.kind)||typed){if(!enabled||!apprenticeship_command_kind(result.kind)||!typed)return common_failure("InvalidApprenticeshipReceipt",row.first);auto source=read_apprenticeship_source_for_result(w.common,w.family_apprenticeship,result);if(!source.ok)return {false,{},source.error};}
        // 师承只由本人独立零时请求办理，任何半日完成嵌套不得冒双方决定。
        if(result.details)if(const auto* advance=std::get_if<AdvanceDetails>(&result.details->data))for(const auto& completed:advance->completed_actions.rows())if(completed.second.family&&completed.second.family->fact_id&&completed.second.family->fact_id->rfind("apprenticeship:",0)==0)return common_failure("ApprenticeshipDirectDecisionRequired",row.first);
    }
    return common_success();
}
Status bind_family_apprenticeship_result_world(WorldBundle& w,const FamilyApprenticeshipState* before,const StableId& actor,Integer sequence,const CommitResult& result){
    if(!w.family_apprenticeship_enabled())return before?common_failure("ApprenticeshipProfileRequired"):common_success();
    if(!before)return common_failure("ApprenticeshipBeforeStateRequired");
    return bind_apprenticeship_result_in_candidate(w.common,*before,w.family_apprenticeship,actor,sequence,result);
}
}

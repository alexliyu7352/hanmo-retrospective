// 三个身份入口复用同一闭合profile与marker依赖，不从课程当前XP或出生头猜测能力。
#include "innate_study_profile.hpp"
#include "resident/storage/common_codec.hpp"
#include "succession_personal_assets.hpp"
#include <algorithm>
#include <map>
namespace hanmo::resident {
Outcome<bool> decode_innate_learning_profile(const BoundaryValue& scenario,const std::vector<StableId>& caps){try{using namespace boundary;
    const auto* config=scenario.find("family_innate_learning");if(!config)return {true,false,{}};
    auto unique=validate_succession_json_unique(*config);if(!unique.ok)return {false,{},unique.error};
    fields(*config,{"version","profile"},{},"family_innate_learning");
    if(get_int(*config,"version","")!=1||get_string(*config,"profile","")!="lineage_per_lesson_v1")fail("UnsupportedInnateLearningProfile","family_innate_learning");
    if(!scenario.find("dynamic_world")||!scenario.find("family_birth")||!scenario.find("family_lineage")||get_string(scenario,"skill_profile","")!="full_sixteen_v1"||std::find(caps.begin(),caps.end(),"family_innate_learning_v1")==caps.end()||std::find(caps.begin(),caps.end(),"cultivation")==caps.end())fail("InnateLearningProfileRequired","family_innate_learning");
    return {true,true,{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status validate_innate_learning_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& versions){
    std::map<StableId,StableId> actual;for(const auto& row:versions)if(!actual.emplace(row).second)return common_failure("InnateLearningDuplicateMarker");
    const auto marker=actual.find("family_innate_learning");if((marker!=actual.end())!=enabled||(marker!=actual.end()&&marker->second!="1"))return common_failure("InnateLearningMarkerMismatch");
    if(!enabled)return common_success();for(const auto* name:{"family_birth","family_lineage","family_life","skills_extended","study_segments"}){const auto at=actual.find(name);if(at==actual.end()||at->second!="1")return common_failure("InnateLearningDependencyRequired",name);}
    const auto dynamic=actual.find("dynamic_world");return dynamic!=actual.end()&&dynamic->second=="local_society_v1"?common_success():common_failure("InnateLearningDependencyRequired","dynamic_world");
}
}

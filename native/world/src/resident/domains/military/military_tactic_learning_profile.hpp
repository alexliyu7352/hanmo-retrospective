// 16不再占用已满uint32位；真实学习账与Growth课程成对显式选择，1..15保原布局。
#pragma once
#include "military_command_resources_profile.hpp"
namespace hanmo::resident {
inline constexpr const char* military_tactic_learning_module="military_tactic_learning";
inline Status validate_military_tactic_learning_modules(const std::vector<std::pair<StableId,StableId>>& modules){
 auto checked=validate_military_command_resources_modules(modules);if(!checked.ok)return checked;unsigned selected=0,lessons=0,prior=0;
 for(const auto& m:modules){if(m.first==military_tactic_learning_module){if(m.second!="1")return common_failure("MilitaryTacticLearningProfileMismatch");++selected;}if(m.first=="growth_tactic_lessons"){if(m.second!="1")return common_failure("MilitaryTacticLessonProfileMismatch");++lessons;}if(m.first==military_command_resources_module&&m.second=="1")++prior;}
 if(selected>1||lessons>1||selected!=lessons)return common_failure("MilitaryTacticLearningPairRequired");return selected&&prior!=1?common_failure("MilitaryTacticLearningPrerequisiteRequired"):common_success();
}
inline Status validate_military_tactic_learning_checkpoint_layout(std::uint32_t version,const std::vector<std::pair<StableId,StableId>>& modules){
 auto checked=validate_military_tactic_learning_modules(modules);if(!checked.ok)return checked;const bool present=std::find(modules.begin(),modules.end(),std::make_pair(StableId{military_tactic_learning_module},StableId{"1"}))!=modules.end();
 if(version<1||version>16||(version==16)!=present)return common_failure("MilitaryTacticLearningCodecVersionMismatch");return validate_military_command_resources_checkpoint_layout(version==16?15u:version,modules);
}
}

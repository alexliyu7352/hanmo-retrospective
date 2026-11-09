// 原Military/M9史之后验证完整野外回放；标记或永久证明删改均不能跳过此门。
#include "military_rout_integration.hpp"
#include "world_bundle.hpp"
#include "resident/domains/military/military_rout_history.hpp"
#include "resident/domains/military/military_rout_profile.hpp"
#include "resident/domains/military/military_command_shock_profile.hpp"
#include <algorithm>
namespace hanmo::resident {
Status validate_military_rout_world(const WorldBundle& w){
 const bool marked=std::any_of(w.catalog->identity.module_versions.begin(),w.catalog->identity.module_versions.end(),[](const auto& m){return m.first==military_rout_module;});if(marked!=w.military_rout_enabled())return common_failure("MilitaryRoutProfileMismatch");
 const bool shock=std::any_of(w.catalog->identity.module_versions.begin(),w.catalog->identity.module_versions.end(),[](const auto& m){return m.first==military_command_shock_module;});if(shock!=w.military_command_shock_enabled())return common_failure("MilitaryCommandShockProfileMismatch");
 auto layout=validate_military_command_shock_modules(w.catalog->identity.module_versions);if(!layout.ok)return layout;
 if(!w.catalog->military)return w.military.rout.entries.empty()&&w.military.rout.rounds.empty()&&w.military.rout.field_order.empty()?common_success():common_failure("UnexpectedMilitaryRoutHistory");
 auto access=w.services.military_access();return validate_military_rout_history(w.common,w.military,*w.catalog->military,access);
}
}

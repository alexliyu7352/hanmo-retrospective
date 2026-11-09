// inspect_known_checkpoint真实共用门：7..14正确全链与缺marker/未来版本直接验证，不复制条件。
#include "resident/domains/military/military_siege_engineering_profile.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
int main(){try{int checks=0;auto check=[&](bool ok){++checks;if(!ok)throw std::runtime_error("layout mismatch at "+std::to_string(checks));};std::vector<std::pair<StableId,StableId>> modules;
 for(std::uint32_t v=1;v<=14;++v){
  if(v==5)modules.emplace_back("military_requests","1");
  if(v==6){modules.emplace_back(military_officer_module,"1");modules.emplace_back(military_officer_political_module,"1");}
  if(v==7){modules.emplace_back(military_field_command_module,"1");modules.emplace_back(military_field_command_political_module,"1");}
  if(v==8){modules.emplace_back("personal_injury_recovery","1");modules.emplace_back(military_recovery_module,"1");}
  if(v==9)modules.emplace_back(military_supply_attrition_module,"1");if(v==10)modules.emplace_back(military_rout_module,"1");if(v==11)modules.emplace_back(military_command_shock_module,"1");if(v==12)modules.emplace_back(military_garrison_rest_module,"1");if(v==13)modules.emplace_back(military_combat_conditions_module,"1");if(v==14)modules.emplace_back(military_commanded_damage_module,"1");
  check(validate_military_commanded_checkpoint_layout(v,modules).ok);if(v>=5){auto missing=modules;missing.pop_back();check(!validate_military_commanded_checkpoint_layout(v,missing).ok);}if(v>=7)check(!validate_military_commanded_checkpoint_layout(v-1,modules).ok);
 }
 check(!validate_military_commanded_checkpoint_layout(0,{}).ok);check(!validate_military_commanded_checkpoint_layout(15,modules).ok);auto duplicate=modules;duplicate.push_back(duplicate.back());check(!validate_military_commanded_checkpoint_layout(14,duplicate).ok);modules.back().second="2";check(!validate_military_commanded_checkpoint_layout(14,modules).ok);
 // 现inspect真实15门须保完整先验，末位只由显式无符号特征承担。
 modules.back().second="1";modules.emplace_back(military_sequential_module,"1");
 check(validate_military_command_resources_checkpoint_layout(14,modules).ok);
 modules.emplace_back(military_command_resources_module,"1");
 check(validate_military_command_resources_checkpoint_layout(15,modules).ok);
 check(!validate_military_command_resources_checkpoint_layout(14,modules).ok);
 check(!validate_military_command_resources_checkpoint_layout(16,modules).ok);
 auto no_sequential=modules;no_sequential.erase(no_sequential.end()-2);check(!validate_military_command_resources_checkpoint_layout(15,no_sequential).ok);
 auto no_commanded=modules;no_commanded.erase(no_commanded.end()-3);check(!validate_military_command_resources_checkpoint_layout(15,no_commanded).ok);
 auto repeated=modules;repeated.push_back(repeated.back());check(!validate_military_command_resources_checkpoint_layout(15,repeated).ok);
 auto unknown=modules;unknown.back().second="2";check(!validate_military_command_resources_checkpoint_layout(15,unknown).ok);
 check(military_command_resources_checkpoint_feature==std::uint32_t{0x80000000u});
 // 真实标题总门依次接16/17，完整选择与缺失任意层均直接消费产品口。
 check(validate_military_siege_engineering_checkpoint_layout(15,modules).ok);
 modules.emplace_back(military_tactic_learning_module,"1");modules.emplace_back("growth_tactic_lessons","1");
 check(validate_military_siege_engineering_checkpoint_layout(16,modules).ok);
 modules.emplace_back(military_siege_engineering_module,"1");
 check(validate_military_siege_engineering_checkpoint_layout(17,modules).ok);
 check(!validate_military_siege_engineering_checkpoint_layout(16,modules).ok);
 check(!validate_military_siege_engineering_checkpoint_layout(18,modules).ok);
 // 原5请求模块对6及以上并非必需，不把测试自造依赖加到旧档。
 auto no_requests17=modules;no_requests17.erase(no_requests17.begin());check(validate_military_siege_engineering_checkpoint_layout(17,no_requests17).ok);
 for(std::size_t i=1;i<modules.size();++i){auto missing=modules;missing.erase(missing.begin()+i);check(!validate_military_siege_engineering_checkpoint_layout(17,missing).ok);}
 auto repeated17=modules;repeated17.push_back(repeated17.back());check(!validate_military_siege_engineering_checkpoint_layout(17,repeated17).ok);
 auto unknown17=modules;unknown17.back().second="2";check(!validate_military_siege_engineering_checkpoint_layout(17,unknown17).ok);

 std::cout<<"MILITARY_COMMANDED_LAYOUT="<<checks<<" FAILURES=0 SCOPE=ACTUAL_CHECKPOINT_LAYOUT_GATE\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

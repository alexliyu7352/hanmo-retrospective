// 只测布局与独立bit，不把新段布局通过冒充实际World可用。
#include "resident/domains/local_economy/supply/supply_policy_profile.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
int main(){std::vector<std::pair<StableId,StableId>> modules={{"local_economy","1"},{"tasks","1"},{"affairs","1"},{"political_takeover","1"}};assert(validate_economic_supply_layout(false,modules).ok);assert(!validate_economic_supply_layout(true,modules).ok);modules.push_back({"economic_supply_policies","1"});assert(!validate_economic_supply_layout(true,modules).ok);modules.push_back({"political_supply_adoptions","1"});assert(validate_economic_supply_layout(true,modules).ok);assert(!validate_economic_supply_layout(false,modules).ok);modules.back().second="2";assert(!validate_economic_supply_layout(true,modules).ok);modules.back().second="1";modules.push_back(modules.back());assert(!validate_economic_supply_layout(true,modules).ok);static_assert((economic_supply_policy_checkpoint_feature&32u)==0);std::cout<<"supply explicit pair 7/0\n";}

// 模块只选择新来源投影，不补旧银行缺少的Need，也不改变原Affairs载荷字节。
#include "resident/domains/local_economy/fiscal/occupation_fiscal_need_profile.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
int main(){
 std::vector<std::pair<StableId,StableId>> old{{"dynamic_world","local_society_v1"},{"political_occupation_fiscal","1"},{"occupation_fiscal_settlement","1"},{"fiscal_settlement","1"},{"fiscal_need_policies","1"},{"affairs","3"}};
 assert(validate_occupation_fiscal_need_layout(false,old).ok&&!validate_occupation_fiscal_need_layout(true,old).ok);auto current=old;current.emplace_back(occupation_fiscal_need_module,"1");assert(validate_occupation_fiscal_need_layout(true,current).ok&&!validate_occupation_fiscal_need_layout(false,current).ok);
 auto bad=current;bad.back().second="2";assert(!validate_occupation_fiscal_need_layout(true,bad).ok);bad=current;bad.push_back(bad.back());assert(!validate_occupation_fiscal_need_layout(true,bad).ok);
 for(std::size_t n=0;n<old.size();++n){bad=current;bad.erase(bad.begin()+n);assert(!validate_occupation_fiscal_need_layout(true,bad).ok);bad=current;bad.push_back(old[n]);assert(!validate_occupation_fiscal_need_layout(true,bad).ok);}
 current[5].second="4";assert(validate_occupation_fiscal_need_layout(true,current).ok);current[5].second="2";assert(!validate_occupation_fiscal_need_layout(true,current).ok);
 for(const auto& raw:{R"({"enabled":true,"version":1})",R"({"enabled":false,"version":1})",R"({"enabled":true})",R"({"enabled":true,"version":true})",R"({"enabled":true,"version":2})",R"({"enabled":true,"version":1,"extra":0})"}){auto value=parse_boundary_json(raw);assert(value.ok);const bool expected=std::string(raw)==R"({"enabled":true,"version":1})";assert(decode_occupation_fiscal_need_profile(value.value).ok==expected);}
 std::cout<<"occupation fiscal needs profile: explicit marker, old pair unchanged, missing/duplicate/version/dependency refusal PASS\n";
}

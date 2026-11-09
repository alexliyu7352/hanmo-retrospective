#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
// Explicit physical successors. The selected original regional base is unchanged.
constexpr bool social_time2_version(Integer v) noexcept {return v==3||v==4||v==5||v==6||v==7||v==8;}
constexpr bool social_knowledge_cut_version(Integer v) noexcept {return v==5||v==6||v==7||v==8;}
constexpr bool social_regional_time2_version(Integer v) noexcept {return v==4||v==6||v==8;}
constexpr Integer social_time2_original_base(Integer v) noexcept {return v==3||v==5||v==7?1:v==4||v==6||v==8?2:0;}
constexpr bool social_propagation_cut_version(Integer v) noexcept {return v==7||v==8;}
// Author choice is explicit; selecting exact time alone does not upgrade old data.
Outcome<Integer> select_social_time2_physical_version(Integer original_base,bool knowledge_common_cut);
Outcome<Integer> select_social_time2_physical_version(Integer original_base,bool knowledge_common_cut,bool propagation_adoption_cut);
}

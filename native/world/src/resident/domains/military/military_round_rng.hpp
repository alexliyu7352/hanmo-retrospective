// 海陆会战共用原PCG轮次叶；只推进已保存state，不重播种、不新建随机流。
#pragma once
#include "resident/model/common_rng.hpp"
#include <charconv>
namespace hanmo::resident {
inline Outcome<Integer> military_round_random_draw(StableId& seed,StableId& state,Integer low,Integer high){
 Integer s=0,v=0;auto a=std::from_chars(seed.data(),seed.data()+seed.size(),s),b=std::from_chars(state.data(),state.data()+state.size(),v);
 if(a.ec!=std::errc()||a.ptr!=seed.data()+seed.size()||b.ec!=std::errc()||b.ptr!=state.data()+state.size()||std::to_string(s)!=seed||std::to_string(v)!=state)return {false,{},common_failure("InvalidMilitaryRandomState").error};
 auto r=godot_pcg32_range(v,low,high);if(!r.ok)return {false,{},r.error};state=std::to_string(r.value.next_state);return {true,r.value.value,{}};
}
}

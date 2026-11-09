// 固定Godot4.7.2的PCG32整数范围叶；这里只用无符号精确位运算，不引入新随机算法。
// PCG32 core adapted from (c) 2014 M. E. O'Neill, Apache-2.0.
// Godot range-wrapper semantics: Godot Engine contributors, MIT.
// Sources and both license texts: common_rng_LICENSE.txt; pinned commit ed1daf0bf001b61586d9930840f2f1394092c079.
#pragma once
#include "common_model.hpp"
#include <cstring>
#include <limits>
namespace hanmo::resident {
struct RngDraw { Integer value=0,next_state=0;std::uint64_t raw_draws=0; };
namespace rng_detail {
inline constexpr std::uint64_t multiplier=6364136223846793005ULL;
inline constexpr std::uint64_t sequence=1442695040888963407ULL;
inline constexpr std::uint64_t increment=(sequence<<1u)|1u;
// Godot将uint64状态经Variant int64保存；memcpy保留二进制而不依赖越界有符号强转。
inline std::uint64_t unsigned_bits(Integer value){static_assert(sizeof(Integer)==sizeof(std::uint64_t));std::uint64_t bits;std::memcpy(&bits,&value,sizeof(bits));return bits;}
inline Integer signed_bits(std::uint64_t bits){Integer value;std::memcpy(&value,&bits,sizeof(value));return value;}
// 原XSH-RR先推进64位state，再从旧state计算32位输出；旋转量为0时也不发生32位移位。
inline std::uint32_t raw(std::uint64_t& state){const auto old=state;state=old*multiplier+(increment|1u);const auto word=static_cast<std::uint32_t>(((old>>18u)^old)>>27u);const auto rotation=static_cast<std::uint32_t>(old>>59u);return (word>>rotation)|(word<<((0u-rotation)&31u));}
}
// 原set_seed的两次初始化推进只用于新流构造/核验；恢复state后抽样绝不重做这一步。
inline Integer godot_pcg32_seed_state(Integer seed){std::uint64_t state=0;rng_detail::raw(state);state+=rng_detail::unsigned_bits(seed);rng_detail::raw(state);return rng_detail::signed_bits(state);}
// Godot原C++入口是signed32端点；倒序允许、相同端点零抽样、全signed32范围走单次raw分支。
inline Outcome<RngDraw> godot_pcg32_range(Integer saved_state,Integer from,Integer to){
    constexpr Integer low=std::numeric_limits<std::int32_t>::min(),high=std::numeric_limits<std::int32_t>::max();
    if(from<low||from>high||to<low||to>high)return {false,{},common_failure("UnsupportedNativeCapability","rng/randi_range_bounds",ErrorKind::unsupported_capability).error};
    if(from==to)return {true,{from,saved_state,0},{}};
    const Integer minimum=std::min(from,to),maximum=std::max(from,to);
    const auto difference=static_cast<std::uint32_t>(maximum-minimum);
    std::uint64_t state=rng_detail::unsigned_bits(saved_state),draws=0;std::uint32_t value=0;
    if(difference==std::numeric_limits<std::uint32_t>::max()){value=rng_detail::raw(state);draws=1;}
    else {const std::uint32_t bound=difference+1u;const std::uint32_t threshold=(0u-bound)%bound;do{value=rng_detail::raw(state);++draws;}while(value<threshold);value%=bound;}
    return {true,{minimum+static_cast<Integer>(value),rng_detail::signed_bits(state),draws},{}};
}
} // 纯叶不读系统时间、不随机化seed、不触及另一个世界或随机流。

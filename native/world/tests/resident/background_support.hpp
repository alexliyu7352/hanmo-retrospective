#pragma once
// 冻结真实oracle的C域冷装载/typed检查点/原序边界测试，不冒称完整世界重放。
#include "resident/domains/military/military_codec.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/domains/military/military_schedule.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace hanmo::resident;
inline BoundaryValue read_file(const std::string& path){std::ifstream f(path);assert(f.good());std::string bytes((std::istreambuf_iterator<char>(f)),{});auto r=parse_boundary_json(bytes);if(!r.ok){std::cerr<<r.error.original.code<<":"<<r.error.original.path<<"\n";std::abort();}return std::move(r.value);}
template<class T> T checked(Outcome<T> r){if(!r.ok){std::cerr<<r.error.original.code<<":"<<r.error.original.path<<":"<<r.error.command<<"\n";std::abort();}return std::move(r.value);}
inline void require(Status r){if(!r.ok){std::cerr<<r.error.original.code<<":"<<r.error.original.path<<":"<<r.error.command<<"\n";std::abort();}}
inline void equal(const BoundaryValue& a,const BoundaryValue& b,const std::string& label){auto diff=compare_boundary(a,b);if(!diff.exact()){auto failure=diff.value_difference?diff.value_difference:diff.order_difference;std::cerr<<label<<":"<<failure->path<<":"<<failure->reason<<"\n";std::abort();}}
// 导出片段按A全局事实序逐条比值；公共结果不属于此测试的C域证明。
inline void compare_fragment(const BoundaryValue& source,const BoundaryValue& fragment){for(auto& row:*fragment.as<BoundaryValue::Dict>()){if(row.first=="action_params"){for(auto& pair:*row.second.as<BoundaryValue::Dict>())equal(*source.find("actions")->find(pair.first)->find("params"),pair.second,"action_params/"+pair.first);}else if(row.first=="facts"){for(auto& pair:*row.second.as<BoundaryValue::Dict>())equal(*source.find("facts")->find(pair.first),pair.second,"facts/"+pair.first);}else equal(*source.find(row.first),row.second,row.first);}}
// A的结果receipt codec交中央所有；诊断装载仅清空该非C字段，不写原始输入。
inline CommonState domain_common(const BoundaryValue& source,const CommonCatalog& cat){auto projection=source;projection.find("results")->data=BoundaryValue::Dict{};return checked(decode_common_seed(projection,cat));}

// 实际旅程各边界的A域codec门：只剥离中央回执，不冒充完整世界安装或行旅执行。
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/social/social_model.hpp"
#include "resident/domains/estate/estate_model.hpp"
#include <fstream>
#include <iostream>
// 发布优化构建也必须执行本测试断言，不能让NDEBUG形成假绿。
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace hanmo::resident;
BoundaryValue read(const char* path){std::ifstream f(path);auto r=parse_boundary_json(std::string((std::istreambuf_iterator<char>(f)),{}));assert(r.ok);return std::move(r.value);}
void exact(const BoundaryValue& expected,const BoundaryValue& actual,const StableId& label){auto diff=compare_boundary(expected,actual);if(!diff.exact()){std::cerr<<label<<":"<<(diff.value_difference?diff.value_difference->path:diff.order_difference->path)<<"\n";std::abort();}}
int main(int argc,char** argv){assert(argc>2);auto raw=read(argv[1]);auto catalog=decode_common_catalog(*raw.find("data"));assert(catalog.ok);auto estates=decode_estate_catalog(*raw.find("data"),catalog.value);assert(estates.ok);auto shared=std::make_shared<const CommonCatalog>(catalog.value);for(int i=2;i<argc;++i){auto source=read(argv[i]);source.find("results")->data=BoundaryValue::Dict{};auto state=decode_common_seed(source,catalog.value);auto social=decode_social_seed(source);auto estate=decode_estate_seed(source);if(!state.ok||!social.ok||!estate.ok){std::cerr<<argv[i]<<":"<<(!state.ok?state.error.original.code:!social.ok?social.error.original.code:estate.error.original.code)<<"\n";return 1;}CommonStore c(state.value,shared);auto sv=validate_social(c,social.value,*shared),ev=validate_estate(c,estate.value,estates.value,*shared);if(!sv.ok||!ev.ok){std::cerr<<argv[i]<<":"<<(!sv.ok?sv.error.original.code:ev.error.original.code)<<"\n";return 1;}auto common=encode_common_state(state.value);for(auto& row:std::get<BoundaryValue::Dict>(common.data)){if(row.first=="actions"||row.first=="facts")continue;exact(*source.find(row.first),row.second,row.first);}auto se=encode_social_state(social.value);for(auto& row:std::get<BoundaryValue::Dict>(se.data))exact(*source.find(row.first),row.second,row.first);auto es=encode_estate_state(estate.value);exact(*source.find("estates"),*es.find("estates"),"estates");for(auto& fact:std::get<BoundaryValue::Dict>(es.find("facts")->data))exact(*source.find("facts")->find(fact.first),fact.second,fact.first);CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);assert(visit_common_checkpoint(writer,state.value).ok&&writer.finish().ok);CommonState restored;const auto& in=bytes;CodecVisitor reader(in);assert(visit_common_checkpoint(reader,restored).ok&&reader.finish().ok);exact(common,encode_common_state(restored),"checkpoint");}std::cout<<"state_record_roundtrip: PASS ("<<(argc-2)<<" actual journey boundaries, A-owned values/key order/validators/checkpoint; central receipt/domain action payload excluded)\n";}

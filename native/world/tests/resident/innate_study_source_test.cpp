// 仅先天学习数学与坏档边界，不借旧World库运行新Growth布局。
#include "resident/domains/family/innate_study_source.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int count=0;void check(bool v,const char* text){++count;if(!v)throw std::runtime_error(text);}}
int main(){try{
    for(Integer base=0;base<=20;++base)for(Integer bonus:{0,500,1000,1500}){auto r=innate_study_lesson_xp(base,bonus);check(r.ok&&r.value==base+base*bonus/10000&&r.value>=base&&r.value<=23,"bounded original lesson");}
    check(!innate_study_lesson_xp(20,1501).ok&&!innate_study_lesson_xp(-1,500).ok,"invalid base or bonus");
    InnateStudySource s;s.actor_id="actor:8";s.skill_id="medicine";s.origin_kind="biological_birth";s.origin_fact_id="family_birth:7";s.observed_tick=12960;s.bonus_bp=1500;s.known_levels.insert("insight",3);
    auto json=encode_innate_study_source(s);auto cold=decode_innate_study_source(json);check(cold.ok&&innate_study_source_equal(s,cold.value),"exact source JSON");
    auto bad=json;bad.as<BoundaryValue::Dict>()->emplace_back("bonus_bp",BoundaryValue{Integer{0}});check(!decode_innate_study_source(bad).ok,"duplicate root source");
    bad=json;bad.find("known_levels")->as<BoundaryValue::Dict>()->emplace_back("insight",BoundaryValue{Integer{0}});check(!decode_innate_study_source(bad).ok,"duplicate nested original level");
    bad=json;bad.find("bonus_bp")->data=Integer{1501};check(!decode_innate_study_source(bad).ok,"excess source bonus");
    auto unknown=s;unknown.origin_kind="legacy_creation_unknown";unknown.origin_fact_id=unknown.actor_id;check(!decode_innate_study_source(boundary::obj({{"actor_id",BoundaryValue{unknown.actor_id}},{"skill_id",BoundaryValue{unknown.skill_id}},{"origin_kind",BoundaryValue{unknown.origin_kind}},{"origin_fact_id",BoundaryValue{unknown.actor_id}},{"observed_tick",BoundaryValue{Integer{1}}},{"bonus_bp",BoundaryValue{Integer{500}}},{"known_levels",boundary::obj({})}})).ok,"unknown creation guessed bonus");
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);check(visit_innate_study_source(writer,cold.value).ok&&writer.finish().ok,"typed source writer");
    const auto original=bytes;const auto& input=original;CodecVisitor reader(input);InnateStudySource restored;check(visit_innate_study_source(reader,restored).ok&&reader.finish().ok&&innate_study_source_equal(s,restored),"typed original source cold");
    auto corrupt=restored;corrupt.field_order.push_back("extra");CodecVisitor::Buffer rejected;CodecVisitor invalid(rejected);check(!visit_innate_study_source(invalid,corrupt).ok,"binary unknown field metadata");
    bytes.push_back(1);const auto& trailing=bytes;CodecVisitor extra(trailing);InnateStudySource tail;check(visit_innate_study_source(extra,tail).ok&&!extra.finish().ok,"trailing bytes");
    for(std::size_t n=0;n<original.size();++n){CodecVisitor::Buffer truncated(original.begin(),original.begin()+n);const auto& raw=truncated;CodecVisitor r(raw);InnateStudySource partial;auto status=visit_innate_study_source(r,partial);check(!status.ok||!r.finish().ok,"truncated source accepted");}
    std::cout<<"INNATE_STUDY_SOURCE "<<count<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"INNATE_STUDY_SOURCE "<<count<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}

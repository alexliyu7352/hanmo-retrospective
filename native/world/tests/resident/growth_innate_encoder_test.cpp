// 精确五开关入口及先天课程双侧拒门；不把句法通过当作运行或完整World通过。
#include "resident/domains/growth/growth_model.hpp"
#include "resident/storage/common_codec.hpp"
#include <type_traits>
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
static_assert(std::is_same_v<decltype(&encode_growth_state),BoundaryValue(*)(const GrowthState&,bool,bool,bool,bool,bool)>);
namespace {int checks=0;
void check(bool v,const char* label){++checks;if(!v)throw std::runtime_error(label);}
bool rejected(const GrowthState& state,bool enabled){try{(void)encode_growth_state(state,false,false,false,false,enabled);return false;}catch(const boundary::DecodeFailure&){return true;}}
}
int main(){try{
    GrowthState empty;check(compare_boundary(encode_growth_state(empty),encode_growth_state(empty,false,false,false,false,true)).exact(),"empty old bytes changed");
    StudyFact f;f.terms.settlement_policy="per_lesson_v1";GrowthState old;old.facts.insert("action:1",f);check(compare_boundary(encode_growth_state(old),encode_growth_state(old,false,false,false,false,true)).exact(),"ordinary old study bytes changed");
    f.terms.settlement_policy="innate_per_lesson_v1";InnateStudySource source;source.actor_id="student";source.skill_id="naval";source.origin_kind="authored_lineage";source.origin_fact_id="";f.terms.innate_source=source;
    GrowthState facts;facts.facts.insert("action:1",f);check(rejected(facts,false),"disabled fact accepted");check(!rejected(facts,true),"enabled fact rejected");
    GrowthState active;GrowthAction action;action.quote.data=f.terms;active.actions.insert("action:1",action);check(rejected(active,false),"disabled action accepted");check(!rejected(active,true),"enabled action rejected");
    f.terms.innate_source.reset();*facts.facts.find("action:1")=f;check(rejected(facts,false)&&rejected(facts,true),"policy with no source accepted");
    f.terms.innate_source=source;f.terms.settlement_policy="per_lesson_v1";*facts.facts.find("action:1")=f;check(rejected(facts,false)&&rejected(facts,true),"source with ordinary policy accepted");
    std::cout<<"GROWTH_INNATE_ENCODER "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"GROWTH_INNATE_ENCODER "<<checks<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}

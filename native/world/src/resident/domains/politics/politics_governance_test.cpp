// 安民规则使用S4真实XP换级函数，证明等待拨付不会重新取当前能力或误把XP当等级。
#include "politics_governance_contract.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <iostream>
#include <cstdlib>
using namespace hanmo::resident;
// Release仍执行具名断言。
void check(bool value,const char* message){if(!value){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
// 非预期规则错误带原码停止测试。
template<class T>T need(Outcome<T> value){if(!value.ok){std::cerr<<value.error.original.code<<'\n';std::exit(1);}return std::move(value.value);}
int main(){
    CommonStore common{CommonState{},std::make_shared<CommonCatalog>()};ActorRecord actor;actor.id="executor";actor.alive=actor.available=true;actor.skills.rites=650;common.state().actors.insert(actor.id,actor);
    AppeasementRule rule{"appease","gdd-06-6.8-v1","appease.name","millet",6,120,10,3,100};
    const auto before=encode_common_state(common.state());DynamicStepInput step;step.from_tick=0;step.to_tick=1;
    auto plan=need(freeze_appeasement_outcome(common,rule,actor.id,1,&step));check(plan.rites_xp==650&&plan.rites_level==3&&plan.support_delta==6,"650 XP means level3 and gain6");need(validate_governance_outcome_plan(rule,plan));
    check(compare_boundary(before,encode_common_state(common.state())).exact(),"freeze changes no world fact, RNG, money or support");
    common.state().actors.find(actor.id)->skills.rites=1200;need(validate_governance_outcome_plan(rule,plan));check(plan.support_delta==6,"late payment cannot improve earlier work snapshot");
    check(!freeze_appeasement_outcome(common,rule,actor.id,1).ok,"ordinary caller cannot freeze future completion");step.to_tick=2;check(!freeze_appeasement_outcome(common,rule,actor.id,2,&step).ok,"only real single-step phase accepted");
    auto forged=plan;forged.support_delta=653;check(!validate_governance_outcome_plan(rule,forged).ok,"raw XP cannot replace level in frozen gain");forged=plan;forged.rites_level=4;check(!validate_governance_outcome_plan(rule,forged).ok,"snapshot XP and level must agree");
    auto encoded=encode_governance_outcome_plan(plan);check(compare_boundary(encoded,encode_governance_outcome_plan(need(decode_governance_outcome_plan(encoded)))).exact(),"frozen plan exact strict codec");
    auto source=encode_appeasement_rule(rule);check(compare_boundary(source,encode_appeasement_rule(need(decode_appeasement_rule(source)))).exact(),"author rule exact codec");std::get<BoundaryValue::Dict>(source.data).emplace_back("completed",BoundaryValue{true});check(!decode_appeasement_rule(source).ok,"author rules cannot claim completed task");
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);need(visit_appeasement_rule(writer,rule));need(visit_governance_outcome_plan(writer,plan));need(writer.finish());AppeasementRule restored_rule;GovernanceOutcomePlan restored_plan;const auto& input=bytes;CodecVisitor reader(input);need(visit_appeasement_rule(reader,restored_rule));need(visit_governance_outcome_plan(reader,restored_plan));need(reader.finish());need(validate_governance_outcome_plan(restored_rule,restored_plan));check(compare_boundary(encode_governance_outcome_plan(plan),encode_governance_outcome_plan(restored_plan)).exact(),"Task-embedded frozen values survive binary checkpoint");restored_rule.version="other";check(!validate_governance_outcome_plan(restored_rule,restored_plan).ok,"later rule version cannot rewrite earned effect");
    std::cout<<"governance outcome PASS: true rites level, fixed work-time reward, captured phase, strict source values; no completion or resource transfer claimed\n";
}

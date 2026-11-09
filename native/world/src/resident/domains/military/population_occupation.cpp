#include "resident/domains/military/military_model.hpp"
// 真实接管窗口是额外禁止条件；到期也不授予军职、人口许可或公共资产权限。
#include "population_occupation.hpp"
#include "population_military_internal.hpp"
namespace hanmo::resident {
namespace {
// 两个入口共用证据形状与原军事占领二验；历史时刻不是当前权限的替代品。
Status proof_valid(const PopulationMilitaryReadContext& c,const PopulationMilitarySource& source,Integer tick,const PoliticalRecruitmentOccupation& proof){
 const auto& now=c.common.state().identity;
 Integer until=0;const auto* occupation=population_military_detail::find(c.military.occupation_facts,proof.occupation_fact_id);
 const auto* header=c.common.state().facts.find(proof.occupation_fact_id);const auto* registration=c.common.state().facts.find(proof.registration_fact_id);
 if(proof.source_identity.world_id!=now.world_id||proof.source_identity.execution_epoch!=now.execution_epoch||proof.source_identity.revision!=now.revision||proof.source_identity.tick!=now.tick||proof.observed_tick!=tick||proof.garrison_id!=source.garrison_id||proof.registration_fact_id.empty()||proof.jurisdiction_id.empty()||proof.site_id.empty()||proof.controlling_organization_id.empty()||proof.occupation_rule.rule_id.empty()||proof.occupation_rule.rule_version<1||proof.occupation_rule.integration_ticks<1||proof.occupation_tick<0||proof.occupation_tick>tick||__builtin_add_overflow(proof.occupation_tick,proof.occupation_rule.integration_ticks,&until)||until!=proof.integration_until_tick||proof.blocked!=(tick<until)||!occupation||!occupation->assets_applied.value_or(false)||occupation->garrison_id!=source.garrison_id||occupation->tick!=proof.occupation_tick||occupation->to_organization_id!=proof.controlling_organization_id||!header||header->kind!="military_occupation"||header->domain!="military"||!registration||registration->kind!="political_public_responsibilities_pending"||registration->domain!="politics")return common_failure("PopulationOccupationSourceMismatch",source.id);
 if(proof.blocked)return common_failure("PopulationRecruitmentOccupationBlocked",proof.occupation_fact_id);
 return common_success();
}
}
// 热门只允许稳定当前或原单步finish，不接受任意未来tick。
Status validate_population_recruitment_occupation(const PopulationMilitaryReadContext& c,const PopulationMilitarySource& source,Integer tick){
 if(!c.recruitment_occupation)return common_success();auto boundary=population_military_detail::boundary_tick(c);if(!boundary.ok)return {false,{},boundary.error};if(tick!=boundary.value)return common_failure("PopulationOccupationPhaseMismatch",source.id);
 auto read=c.recruitment_occupation(source.garrison_id);if(!read.ok)return {false,{},read.error};return read.value?proof_valid(c,source,tick,*read.value):common_success();
}
// 冷读绑定真实成功/失败登记行动或成功入籍结果；不能拿任意Common fact与时间组合探空窗口。
Status validate_population_recruitment_occupation_before(const PopulationMilitaryReadContext& c,const PopulationMilitarySource& source,Integer tick,const StableId& id){
 if(!c.recruitment_occupation&&!c.recruitment_occupation_before)return common_success();if(!c.recruitment_occupation_before)return common_failure("PopulationOccupationHistoryRequired",source.id);
 auto boundary=population_military_detail::boundary_tick(c);if(!boundary.ok)return {false,{},boundary.error};if(tick<0||tick>boundary.value)return common_failure("PopulationOccupationPhaseMismatch",id);
 const auto* header=c.common.state().facts.find(id);if(!header||header->domain!="population_military")return common_failure("PopulationOccupationFactMismatch",id);
 const PopulationMilitaryActionFact* action=nullptr;
 if(header->kind=="population_military_action"){action=c.state.action_facts.find(id);if(!action||action->started_tick!=tick)return common_failure("PopulationOccupationFactMismatch",id);}
 else if(header->kind=="population_enrolled"){const auto* fact=c.state.facts.find(id);action=fact?c.state.action_facts.find(fact->action_id):nullptr;if(!fact||fact->id!=id||fact->kind!="population_enrolled"||fact->tick!=tick||fact->source_id!=source.id||!action||!action->success||action->phase!="completed"||action->completed_tick!=tick||action->receipt_id!=id)return common_failure("PopulationOccupationFactMismatch",id);}
 else return common_failure("PopulationOccupationFactMismatch",id);
 const auto* enroll=std::get_if<PopulationEnrollCommand>(&action->request);if(!enroll||action->kind!="population_enroll"||action->terms.source_id!=source.id||enroll->source_id!=source.id)return common_failure("PopulationOccupationFactMismatch",id);
 auto read=c.recruitment_occupation_before(source.garrison_id,tick,id);if(!read.ok)return {false,{},read.error};return read.value?proof_valid(c,source,tick,*read.value):common_success();
}
}

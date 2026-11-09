#include "growth_skill_api.hpp"
// 只计算有限合同分期，不读取或改写钱包；真正钱款由原Common单一预约/转款原语结算。
#include "growth_tactic_lessons.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {namespace {
bool id(const StableId& text){return !text.empty()&&text.size()<=256;}
bool terms(Integer fee,Integer duration){return fee>=0&&fee<=2147483647&&duration>=1&&duration<=720;}
bool unique_ids(const std::vector<StableId>& ids){std::set<StableId> seen;for(const auto& value:ids)if(!id(value)||!seen.insert(value).second)return false;return true;}
}
Outcome<Integer> growth_tactic_lesson_fee_for_ordinal(Integer fee,Integer duration,Integer ordinal){
    if(!terms(fee,duration)||ordinal<1||ordinal>duration)return {false,0,common_failure("InvalidTacticLessonFeePeriod").error};
    return {true,fee/duration+(ordinal<=fee%duration?1:0),{}};
}
Outcome<Integer> growth_tactic_lesson_fee_through(Integer fee,Integer duration,Integer completed){
    if(!terms(fee,duration)||completed<0||completed>duration)return {false,0,common_failure("InvalidTacticLessonFeePeriod").error};
    // 商乘积不超过原总额，避免fee*completed先乘溢出，也不由余额差猜实际付款。
    return {true,(fee/duration)*completed+std::min(completed,fee%duration),{}};
}
Status validate_growth_tactic_lesson_definition_shape(const GrowthTacticLessonDefinition& d){
    for(const auto* value:{&d.id,&d.tactic_id,&d.teacher_actor_id,&d.dialogue_id,&d.offer_node_id,&d.recipient_wallet_id})if(!id(*value))return common_failure("InvalidTacticLessonReference");
    return terms(d.total_fee,d.duration)?common_success():common_failure("InvalidTacticLessonTerms",d.id);
}
Status validate_growth_tactic_lesson_prerequisite_shape(const GrowthTacticPrerequisite& q){
    if((q.skill_id!="tactics"&&q.skill_id!="infantry"&&q.skill_id!="archery"&&q.skill_id!="cavalry"&&q.skill_id!="transport")||q.required_level<1||q.required_level>4||q.initial_xp<0||q.minimum_xp<q.initial_xp||q.minimum_xp>1200||q.minimum_level!=growth_skill_level(q.minimum_xp)||q.minimum_level<q.required_level||q.observed_tick<0||q.initial_created_tick<0||q.initial_created_tick>q.observed_tick||!id(q.initial_prototype_id)||!unique_ids(q.initial_source_ids)||!unique_ids(q.study_source_ids))return common_failure("InvalidTacticLessonPrerequisite");
    const bool authored=q.origin_kind=="effective_authored_actor";const bool created=q.origin_kind=="character_creation"||q.origin_kind=="adoption_creation"||q.origin_kind=="biological_birth";
    if((!authored&&!created)||(authored&&(!q.initial_source_ids.empty()||q.initial_created_tick!=0))||(created&&q.initial_source_ids.size()!=2))return common_failure("InvalidTacticLessonIdentitySource");
    std::set<StableId> all(q.initial_source_ids.begin(),q.initial_source_ids.end());for(const auto& source:q.study_source_ids)if(!all.insert(source).second)return common_failure("DuplicateTacticLessonQualificationSource");return common_success();
}
GrowthTacticPrerequisite freeze_growth_tactic_lesson_prerequisite(const GrowthSkillQualification& q,Integer required){
    GrowthTacticPrerequisite out;out.skill_id=q.skill_id;out.origin_kind=q.origin_kind;out.initial_prototype_id=q.initial_prototype_id;out.required_level=required;out.observed_tick=q.observed_tick;out.initial_created_tick=q.initial_created_tick;out.initial_xp=q.initial_xp;out.minimum_xp=q.minimum_xp;out.minimum_level=q.minimum_level;out.initial_source_ids=q.initial_source_ids;out.study_source_ids=q.study_source_ids;return out;
}
}

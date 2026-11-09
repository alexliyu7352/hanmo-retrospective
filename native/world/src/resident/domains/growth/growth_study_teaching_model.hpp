// 普通技艺教学的声明与永久来源；不包含Action或新的进度账。
#pragma once
#include "resident/domains/interaction/dialogue_node_source.hpp"
namespace hanmo::resident {
struct GrowthStudyTeachingRequestDefinition {
    StableId id,teacher_actor_id,student_actor_id,skill_id,course_role_id,dialogue_id,node_id;
    Integer duration=0;
    StableId payment_policy,tariff;
};
struct GrowthStudyTeachingRequestCatalog {
    bool enabled=false;
    OrderedTable<GrowthStudyTeachingRequestDefinition> definitions;
};
// 金额和钱包仍唯一保存在原StudyTerms；此来源把明确请求与那份条款绑定。
struct GrowthStudyTeachingAgreement {
    StableId request_id,teacher_actor_id,student_actor_id,skill_id,course_role_id;
    StableId dialogue_id,node_id,node_kind,session_id,payment_policy,tariff;
    InteractionContext request_context;
    Integer duration=0,accepted_tick=0,accepted_revision=0;
    FieldOrder field_order;
};
struct GrowthStudyTeachingRequestSource { AuthoredNodeSourceProof source; GrowthStudyTeachingAgreement agreement; };
bool growth_study_teaching_agreement_equal(const GrowthStudyTeachingAgreement&,const GrowthStudyTeachingAgreement&);
}

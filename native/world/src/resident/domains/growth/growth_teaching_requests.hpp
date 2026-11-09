// 具名学生邀学只绑定真实作者节点；本叶不授能力、不排行动或代写交互会话。
#pragma once
#include "resident/domains/interaction/dialogue_node_source.hpp"
namespace hanmo::resident {
struct GrowthTeachingRequestDefinition {
    StableId id,teacher_actor_id,student_actor_id,skill_id,dialogue_id,node_id;
};
struct GrowthTeachingRequestCatalog {
    bool enabled=false;
    OrderedTable<GrowthTeachingRequestDefinition> definitions;
};
// 永久来源没有execution_epoch；实际结果与action_id继续属于原TrainingFact。
struct GrowthTeachingAgreement {
    StableId request_id,teacher_actor_id,student_actor_id,skill_id;
    StableId dialogue_id,node_id,node_kind,session_id;
    InteractionContext context;
    Integer accepted_tick=0,accepted_revision=0;
    FieldOrder field_order;
};
// 仅在同候选动作占用之前使用；禁止把临时证明编码成跨档许可。
struct GrowthTeachingRequestQuote {
    AuthoredNodeSourceProof source;
    GrowthTeachingAgreement agreement;
};
// 字段顺序不构成不同邀请；比较所有具体来源语义，供原报价与事实交叉核对。
bool growth_teaching_agreement_equal(const GrowthTeachingAgreement&,const GrowthTeachingAgreement&);
Status validate_growth_teaching_request_catalog(const GrowthTeachingRequestCatalog&,const CommonCatalog&,const InteractionCatalog&);
Outcome<GrowthTeachingRequestQuote> preview_growth_teaching_request(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const GrowthTeachingRequestCatalog&,const StableId& teacher,const StableId& student,const StableId& skill);
Status revalidate_growth_teaching_request(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const GrowthTeachingRequestCatalog&,const GrowthTeachingRequestQuote&);
// 仅核永久来源形状/作者绑定；调用方另与原TrainingFact核对，不回查过去会话。
Status validate_growth_teaching_agreement_source(const CommonState&,const CommonCatalog&,const InteractionCatalog&,const GrowthTeachingRequestCatalog&,const GrowthTeachingAgreement&);
}

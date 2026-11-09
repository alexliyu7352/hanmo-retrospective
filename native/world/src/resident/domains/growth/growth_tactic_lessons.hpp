// 显式战法教学只拥有本人报名、真实课时和学费履约；战法掌握仍唯一属于军事领域。
#pragma once
#include "growth_tactic_lesson_qualification.hpp"
#include "resident/domains/interaction/dialogue_provider_source.hpp"
namespace hanmo::resident {
// BA专用课长/学费没有既定统一数值，作者必须逐条明列；上限只是有界输入保护。
struct GrowthTacticLessonDefinition {
    StableId id,tactic_id,teacher_actor_id,dialogue_id,offer_node_id,recipient_wallet_id;
    Integer duration=0,total_fee=0;
};
struct GrowthTacticLessonCatalog {OrderedTable<GrowthTacticLessonDefinition> definitions;};
// 教师取得来源是原Military只读结果的持久引用；不保存或授予另一份战法掌握。
struct GrowthTacticTeacherSource {
    StableId source_kind,acquisition_fact_id,source_fact_id,initial_grant_id;
    Integer acquired_tick=0;
    FieldOrder field_order;
};
// 基础技艺保存可证下界而非今日XP；原人物初始来源与Study贡献均可独立重读。
struct GrowthTacticPrerequisite {
    StableId skill_id,origin_kind,initial_prototype_id;
    Integer required_level=0,observed_tick=0,initial_created_tick=0,initial_xp=0,minimum_xp=0,minimum_level=0;
    std::vector<StableId> initial_source_ids,study_source_ids;
    FieldOrder field_order;
};
// 永久报名保原教师本人say与实际双方；execution_epoch只留在临时报价，不作为冷档身份。
struct GrowthTacticLessonAgreement {
    StableId definition_id,tactic_id,student_id,teacher_id,place_id,payer_wallet_id,recipient_wallet_id;
    StableId dialogue_id,node_id,session_id,scenario_id;
    WorldId world_id{};
    InteractionContext context;
    Integer tick=0,revision=0,duration=0,total_fee=0;
    GrowthTacticTeacherSource teacher_source;
    GrowthTacticPrerequisite prerequisite;
    FieldOrder field_order;
};
// 原提交尾才填写；启动和终止必须绑定当次真实request/result，不能挂接裁剪前同tick旧结果。
struct GrowthTacticLessonCommit {
    StableId result_id,actor_id,kind;
    Integer request_seq=0,revision=0,from_tick=-1;
    // 指本次真实半日前进，不是回执variant；Personal/对白可隐藏原AdvanceDetails。
    bool advance=false;
    FieldOrder field_order;
};
struct GrowthTacticLessonStart {
    StableId kind="tactic_lesson_started",action_id;
    GrowthTacticLessonAgreement agreement;
    GrowthTacticLessonCommit commit;
    FieldOrder field_order;
};
// 每条只记实际半日已提供的教学及原Common预约付款；无虚拟钱包或技能经验增量。
struct GrowthTacticLessonCredit {
    StableId kind="tactic_lesson_credit",started_fact_id,action_id;
    Integer tick=0,ordinal=0,fee_paid=0;
    FieldOrder field_order;
};
struct GrowthTacticLessonClosure {
    StableId kind="tactic_lesson_closed",started_fact_id,action_id;
    // completed/student_cancelled/teacher_unavailable/student_unavailable/student_dead/teacher_dead。
    StableId reason,cause_fact_id;
    Integer tick=0,provided_half_days=0,fee_paid=0,fee_released=0;
    GrowthTacticLessonCommit commit;
    FieldOrder field_order;
};
struct GrowthTacticLessonState {
    OrderedTable<GrowthTacticLessonStart> starts;
    OrderedTable<GrowthTacticLessonCredit> credits;
    OrderedTable<GrowthTacticLessonClosure> closures;
    FieldOrder field_order;
};
struct GrowthTacticLessonQuote {
    AuthoredProviderSourceProof teacher_say;
    GrowthTacticLessonAgreement agreement;
};
// 费用先由原Common全额预约，再随真实课时等额分摊余数；未提供部分只能释放原预约。
Outcome<Integer> growth_tactic_lesson_fee_for_ordinal(Integer total_fee,Integer duration,Integer ordinal);
Outcome<Integer> growth_tactic_lesson_fee_through(Integer total_fee,Integer duration,Integer provided_half_days);
Status validate_growth_tactic_lesson_definition_shape(const GrowthTacticLessonDefinition&);
Status validate_growth_tactic_lesson_prerequisite_shape(const GrowthTacticPrerequisite&);
GrowthTacticPrerequisite freeze_growth_tactic_lesson_prerequisite(const GrowthSkillQualification&,Integer required_level);
}

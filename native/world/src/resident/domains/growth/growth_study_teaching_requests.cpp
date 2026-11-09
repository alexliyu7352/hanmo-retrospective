#include "growth_study_teaching_requests.hpp"
#include "resident/domains/growth/growth_equal.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
#include <algorithm>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
bool id(const StableId& value){return !value.empty()&&value.size()<=256;}
const DialogueDefinition* dialogue(const InteractionCatalog& catalog,const StableId& name){for(const auto& row:catalog.dialogues)if(row.id==name)return &row;return nullptr;}
const DialogueNode* node(const DialogueDefinition& definition,const StableId& name){for(const auto& row:definition.nodes)if(row.id==name)return &row;return nullptr;}

template<class T>Outcome<T> failed(const Status& result){return {false,{},result.error};}
}
bool growth_study_teaching_agreement_equal(const GrowthStudyTeachingAgreement& a,const GrowthStudyTeachingAgreement& b){return std::tie(a.request_id,a.teacher_actor_id,a.student_actor_id,a.skill_id,a.course_role_id,a.dialogue_id,a.node_id,a.node_kind,a.session_id,a.payment_policy,a.tariff,a.duration,a.accepted_tick,a.accepted_revision)==std::tie(b.request_id,b.teacher_actor_id,b.student_actor_id,b.skill_id,b.course_role_id,b.dialogue_id,b.node_id,b.node_kind,b.session_id,b.payment_policy,b.tariff,b.duration,b.accepted_tick,b.accepted_revision)&&growth::context_equal(a.request_context,b.request_context);}
// 显式普通教师课程沿原study资格；不借公职成年守卫或一般台词解释支付意愿。
Status validate_growth_study_teaching_requests(const GrowthStudyTeachingRequestCatalog& requests,const CommonCatalog& shared,const GrowthCatalog& courses,const InteractionCatalog& interaction){
    if(!requests.enabled)return requests.definitions.empty()?common_success():common_failure("StudyTeachingCapabilityRequired");
    if(requests.definitions.empty()||requests.definitions.size()>4096)return common_failure("InvalidStudyTeachingCatalogSize");
    std::set<std::tuple<StableId,StableId,StableId,StableId,StableId,Integer>> bindings;
    for(const auto& row:requests.definitions.rows()){
        const auto& r=row.second;
        for(const auto* value:{&r.id,&r.teacher_actor_id,&r.student_actor_id,&r.skill_id,&r.course_role_id,&r.dialogue_id,&r.node_id})if(!id(*value))return common_failure("InvalidStudyTeachingRequest",row.first);
        if(r.id!=row.first||r.teacher_actor_id==r.student_actor_id||r.duration<2||r.duration>6)return common_failure("InvalidStudyTeachingRequest",row.first);
        if(r.payment_policy!="student_personal_tuition_v1"||r.tariff!="ordinary_next_level_v1")return common_failure("UnsupportedStudyTeachingPayment",row.first);
        for(const auto* actor:{&r.teacher_actor_id,&r.student_actor_id})if(std::find(shared.actor_ids.begin(),shared.actor_ids.end(),*actor)==shared.actor_ids.end())return common_failure("UnknownStudyTeachingActor",*actor);
        if(!growth_skill_defined(r.skill_id,shared.extended_skills))return common_failure("UnknownStudyTeachingSkill",r.skill_id);
        const auto* role=shared.roles.find(r.course_role_id);const auto* course=growth::course(courses,r.course_role_id,r.skill_id);
        if(!role||!course||course->mode!="teacher"||std::find(role->services.begin(),role->services.end(),"study_skill")==role->services.end())return common_failure("StudyTeachingTeacherCourseRequired",row.first);
        const auto* d=dialogue(interaction,r.dialogue_id);const auto* at=d?node(*d,r.node_id):nullptr;
        if(!d||!shared.roles.find(d->context_role)||d->context_role==r.course_role_id||!at||(at->kind!="say"&&at->kind!="choice"))return common_failure("StudyTeachingRequestNodeRequired",row.first);
        if(!bindings.emplace(r.teacher_actor_id,r.student_actor_id,r.skill_id,r.dialogue_id,r.node_id,r.duration).second)return common_failure("AmbiguousStudyTeachingRequest",row.first);
    }
    return common_success();
}
// 先有当前学生明确请求，再借原学生课程报价；此函数不写资源预留或Action。
Outcome<GrowthStudyTeachingRequestSource> preview_growth_study_teaching_source(const GrowthStudyTeachingRead& read,const GrowthStudyTeachingRequestCatalog& requests,const StableId& teacher,const StableId& student,const StableId& skill,Integer duration){
    if(!requests.enabled)return failed<GrowthStudyTeachingRequestSource>(common_failure("StudyTeachingUnavailable"));
    auto valid=validate_growth_study_teaching_requests(requests,read.shared,read.catalog,read.interaction_catalog);if(!valid.ok)return failed<GrowthStudyTeachingRequestSource>(valid);
    const auto source=preview_authored_node_source(read.common,read.interaction,read.interaction_catalog,read.interaction_access,teacher,student);if(!source.ok)return {false,{},source.error};
    const GrowthStudyTeachingRequestDefinition* request=nullptr;
    for(const auto& row:requests.definitions.rows()){const auto& r=row.second;if(std::tie(r.teacher_actor_id,r.student_actor_id,r.skill_id,r.dialogue_id,r.node_id,r.duration)==std::tie(teacher,student,skill,source.value.dialogue_id,source.value.node_id,duration)){request=&r;break;}}
    if(!request)return failed<GrowthStudyTeachingRequestSource>(common_failure("StudyTeachingRequestNotOffered"));
    const auto* role=read.shared.roles.find(request->course_role_id);
    if(source.value.context.kind||source.value.context.place_id!=role->place_id)return failed<GrowthStudyTeachingRequestSource>(common_failure("StudyTeachingMeetingPlaceMismatch"));
    GrowthStudyTeachingAgreement a;a.request_id=request->id;a.teacher_actor_id=teacher;a.student_actor_id=student;a.skill_id=skill;a.course_role_id=request->course_role_id;a.dialogue_id=source.value.dialogue_id;a.node_id=source.value.node_id;a.node_kind=source.value.node_kind;a.session_id=source.value.session_id;a.payment_policy=request->payment_policy;a.tariff=request->tariff;a.request_context=source.value.context;a.duration=duration;a.accepted_tick=source.value.source_identity.tick;a.accepted_revision=source.value.source_identity.revision;
    return {true,{source.value,std::move(a)},{}};
}
Outcome<GrowthStudyTeachingQuote> preview_growth_study_teaching_request(const GrowthStudyTeachingRead& read,const GrowthStudyTeachingRequestCatalog& requests,const StableId& teacher,const StableId& student,const StableId& skill,Integer duration){
    auto source=preview_growth_study_teaching_source(read,requests,teacher,student,skill,duration);if(!source.ok)return {false,{},source.error};
    const auto* role=read.shared.roles.find(source.value.agreement.course_role_id);
    GrowthCommand command;command.kind="study_skill";command.skill_id=skill;command.ticks=duration;command.challenge_mode="automatic";command.context=InteractionContext{role->place_id,teacher,role->id};
    auto course=preview_growth(read.common,read.growth,read.catalog,read.shared,read.social,read.growth_access,student,command);if(!course.ok)return {false,{},course.error};
    const auto* terms=std::get_if<StudyTerms>(&course.value.data);if(!terms)return failed<GrowthStudyTeachingQuote>(common_failure("StudyTeachingCourseRequired"));
    auto valid=validate_growth_study_teaching_agreement(read.common,read.shared,read.catalog,read.interaction_catalog,requests,source.value.agreement,*terms);if(!valid.ok)return failed<GrowthStudyTeachingQuote>(valid);
    return {true,{std::move(source.value.source),std::move(source.value.agreement),std::move(course.value)},{}};
}
Status revalidate_growth_study_teaching_source(const GrowthStudyTeachingRead& read,const GrowthStudyTeachingRequestCatalog& requests,const GrowthStudyTeachingRequestSource& quote){
    const auto& p=quote.source;const auto& a=quote.agreement;
    if(std::tie(p.actor_id,p.provider_actor_id,p.session_id,p.dialogue_id,p.node_id,p.node_kind,p.source_identity.tick,p.source_identity.revision)!=std::tie(a.teacher_actor_id,a.student_actor_id,a.session_id,a.dialogue_id,a.node_id,a.node_kind,a.accepted_tick,a.accepted_revision)||!growth::context_equal(p.context,a.request_context))return common_failure("StudyTeachingSourceMismatch");
    auto valid=revalidate_authored_node_source(read.common,read.interaction,read.interaction_catalog,read.interaction_access,p);if(!valid.ok)return valid;
    const auto fresh=preview_growth_study_teaching_source(read,requests,a.teacher_actor_id,a.student_actor_id,a.skill_id,a.duration);if(!fresh.ok)return {false,{},fresh.error};
    return growth_study_teaching_agreement_equal(a,fresh.value.agreement)?common_success():common_failure("StaleStudyTeachingQuote");
}
// 原报价全量重取，不能用有效Proof A拼接不同学生、时长或费用的报价B。
Status revalidate_growth_study_teaching_request(const GrowthStudyTeachingRead& read,const GrowthStudyTeachingRequestCatalog& requests,const GrowthStudyTeachingQuote& quote){
    const auto& p=quote.source;const auto& a=quote.agreement;
    if(std::tie(p.actor_id,p.provider_actor_id,p.session_id,p.dialogue_id,p.node_id,p.node_kind,p.source_identity.tick,p.source_identity.revision)!=std::tie(a.teacher_actor_id,a.student_actor_id,a.session_id,a.dialogue_id,a.node_id,a.node_kind,a.accepted_tick,a.accepted_revision)||!growth::context_equal(p.context,a.request_context))return common_failure("StudyTeachingSourceMismatch");
    auto valid=revalidate_authored_node_source(read.common,read.interaction,read.interaction_catalog,read.interaction_access,p);if(!valid.ok)return valid;
    const auto fresh=preview_growth_study_teaching_request(read,requests,a.teacher_actor_id,a.student_actor_id,a.skill_id,a.duration);if(!fresh.ok)return {false,{},fresh.error};
    if(!growth_study_teaching_agreement_equal(a,fresh.value.agreement)||!growth::quote_equal(quote.student_course,fresh.value.student_course))return common_failure("StaleStudyTeachingQuote");
    return common_success();
}
// 历史接缝只核请求来源与原StudyTerms绑定；完整课时/费率仍由原StudyFact校验拥有。
Status validate_growth_study_teaching_agreement(const CommonStore& common,const CommonCatalog& shared,const GrowthCatalog& courses,const InteractionCatalog& interaction,const GrowthStudyTeachingRequestCatalog& requests,const GrowthStudyTeachingAgreement& a,const StudyTerms& t){
    if(!requests.enabled)return common_failure("StudyTeachingUnavailable");
    auto valid=validate_growth_study_teaching_requests(requests,shared,courses,interaction);if(!valid.ok)return valid;
    const auto* r=requests.definitions.find(a.request_id);
    if(!r||std::tie(r->teacher_actor_id,r->student_actor_id,r->skill_id,r->course_role_id,r->dialogue_id,r->node_id,r->duration,r->payment_policy,r->tariff)!=std::tie(a.teacher_actor_id,a.student_actor_id,a.skill_id,a.course_role_id,a.dialogue_id,a.node_id,a.duration,a.payment_policy,a.tariff))return common_failure("StudyTeachingAgreementMismatch");
    if(a.accepted_tick<0||a.accepted_tick>common.tick()||a.accepted_revision<0||a.accepted_revision>common.state().identity.revision)return common_failure("InvalidStudyTeachingTime");
    for(const auto& actor:{a.teacher_actor_id,a.student_actor_id})if(!common.actor(actor))return common_failure("UnknownStudyTeachingActor",actor);
    const auto* d=dialogue(interaction,a.dialogue_id);const auto* at=node(*d,a.node_id);const auto* request_role=shared.roles.find(d->context_role);const auto* role=shared.roles.find(a.course_role_id);
    if(!id(a.session_id)||a.node_kind!=at->kind||!face_to_face_contact_context_valid(a.request_context)||a.request_context.kind||a.request_context.provider_actor_id!=a.student_actor_id||a.request_context.role_id!=request_role->id||a.request_context.place_id!=request_role->place_id||a.request_context.place_id!=role->place_id)return common_failure("InvalidStudyTeachingSource");
    if(t.skill_id!=a.skill_id||t.student_id!=a.student_actor_id||t.teacher_id!=a.teacher_actor_id||t.duration!=a.duration||t.mode!="teacher"||t.context.kind||t.context.encounter_id||t.context.input_error||t.context.role_id!=a.course_role_id||t.context.provider_actor_id!=a.teacher_actor_id||t.context.place_id!=role->place_id)return common_failure("StudyTeachingTermsMismatch");
    return common_success();
}
}

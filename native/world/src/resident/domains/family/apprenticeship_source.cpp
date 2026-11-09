#include "resident/kernel/catalog_section_observation.hpp"
// 师承接触、成年与历史权源的纯读；任何未知来源明确失败，不用今天状态替代旧决定。
#include "apprenticeship_source.hpp"
#include "family_continuation_people4.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
#include "resident/domains/growth/growth_skill_api.hpp"
#include <algorithm>
#include <set>
#include <tuple>
namespace hanmo::resident { namespace {
// 引用有限且非空，不接受自由空值作为全体人物许可。
bool id(const StableId& value){return !value.empty()&&value.size()<=256;}
// 师承只声明原十六技艺之一；实际教师资格仍由原课程决定。
bool skill(const StableId& name,const CommonCatalog& catalog){return growth_skill_defined(name,catalog.extended_skills);}
// 只读正规目录原对白和节点，不接受玩家自由文本代替 NPC 本人意愿。
const DialogueDefinition* talk(const InteractionCatalog& c,const StableId& name){for(const auto& d:c.dialogues)if(d.id==name)return &d;return nullptr;}
const DialogueNode* saying(const DialogueDefinition& d,const StableId& name){for(const auto& n:d.nodes)if(n.id==name)return &n;return nullptr;}
// 字段顺序不是语义，但不允许它掩盖不同接触地点或对方。
bool same_context(const InteractionContext& a,const InteractionContext& b){return std::tie(a.place_id,a.role_id,a.provider_actor_id,a.kind,a.encounter_id,a.input_error)==std::tie(b.place_id,b.role_id,b.provider_actor_id,b.kind,b.encounter_id,b.input_error);}
// 静态绑定与永久来源共用；此层不重演原会话或自动证明当时生命资格。
Status source_binding(const CommonStore& c,const CommonCatalog& common,const InteractionCatalog& interactions,const ApprenticeshipCatalog& catalog,const ApprenticeshipStatementSource& s){
    auto checked=validate_apprenticeship_source_shape(s);if(!checked.ok)return checked;
    checked=validate_apprenticeship_catalog(catalog,common,interactions);if(!checked.ok)return checked;
    if(!catalog.enabled)return common_failure("ApprenticeshipProfileRequired");
    const auto* d=catalog.statements.find(s.statement_id);
    if(!d||d->speaker_actor_id!=s.speaker_actor_id||d->skill_id!=s.skill_id||d->dialogue_id!=s.dialogue_id||d->node_id!=s.node_id||d->terms_version!=s.terms_version||(d->listener_actor_id&&*d->listener_actor_id!=s.listener_actor_id))return common_failure("ApprenticeshipStatementMismatch");
    const auto teacher=d->speaker_side=="teacher"?s.speaker_actor_id:s.listener_actor_id;
    const auto student=d->speaker_side=="student"?s.speaker_actor_id:s.listener_actor_id;
    if(s.teacher_actor_id!=teacher||s.student_actor_id!=student)return common_failure("ApprenticeshipParticipantMismatch");
    if(s.world_id!=c.state().identity.world_id||s.scenario_id!=c.state().metadata.scenario_id||s.tick>c.tick()||s.revision>c.state().identity.revision)return common_failure("ApprenticeshipSourceIdentityMismatch");
    const auto* dialogue=talk(interactions,s.dialogue_id);const auto* role=dialogue?common.roles.find(dialogue->context_role):nullptr;
    if(!dialogue||!role||s.context.role_id!=dialogue->context_role||s.context.place_id!=role->place_id||!common.places.find(s.context.place_id))return common_failure("ApprenticeshipContactMismatch");
    return common_success();
}
}
Status validate_apprenticeship_source_shape(const ApprenticeshipStatementSource& s){
    for(const auto* text:{&s.statement_id,&s.speaker_actor_id,&s.listener_actor_id,&s.teacher_actor_id,&s.student_actor_id,&s.skill_id,&s.dialogue_id,&s.node_id,&s.session_id,&s.scenario_id})if(!id(*text))return common_failure("InvalidApprenticeshipReference");
    if(s.speaker_actor_id==s.listener_actor_id||s.teacher_actor_id==s.student_actor_id||s.node_kind!="say"||s.terms_version!=1||s.tick<0||s.tick>2147483647||s.revision<0||!face_to_face_contact_context_valid(s.context)||s.context.kind||s.context.encounter_id||s.context.input_error||s.context.provider_actor_id!=s.speaker_actor_id||std::all_of(s.world_id.begin(),s.world_id.end(),[](auto b){return b==0;}))return common_failure("InvalidApprenticeshipSource");
    const std::set<StableId> keys{"statement_id","speaker_actor_id","listener_actor_id","teacher_actor_id","student_actor_id","skill_id","dialogue_id","node_id","node_kind","session_id","scenario_id","world_id","context","terms_version","tick","revision"};
    auto order=[](const FieldOrder& fields,const std::set<StableId>& wanted){return fields.empty()||(fields.size()==wanted.size()&&std::set<StableId>(fields.begin(),fields.end())==wanted);};
    return order(s.field_order,keys)&&order(s.context.field_order,{"place_id","role_id","provider_actor_id"})?common_success():common_failure("BoundaryFieldOrderMismatch");
}
bool apprenticeship_source_equal(const ApprenticeshipStatementSource& a,const ApprenticeshipStatementSource& b){return std::tie(a.statement_id,a.speaker_actor_id,a.listener_actor_id,a.teacher_actor_id,a.student_actor_id,a.skill_id,a.dialogue_id,a.node_id,a.node_kind,a.session_id,a.scenario_id,a.world_id,a.terms_version,a.tick,a.revision)==std::tie(b.statement_id,b.speaker_actor_id,b.listener_actor_id,b.teacher_actor_id,b.student_actor_id,b.skill_id,b.dialogue_id,b.node_id,b.node_kind,b.session_id,b.scenario_id,b.world_id,b.terms_version,b.tick,b.revision)&&same_context(a.context,b.context);}
Status validate_apprenticeship_catalog(const ApprenticeshipCatalog& catalog,const CommonCatalog& shared,const InteractionCatalog& interactions){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 23, "ApprenticeshipCatalog", "validate_apprenticeship_catalog", &catalog);

    if(!catalog.enabled)return catalog.statements.empty()?common_success():common_failure("ApprenticeshipProfileRequired");
    if(catalog.statements.empty()||catalog.statements.size()>4096)return common_failure("InvalidApprenticeshipCatalogSize");
    auto authored=[&](const StableId& a){return id(a)&&std::find(shared.actor_ids.begin(),shared.actor_ids.end(),a)!=shared.actor_ids.end();};
    std::set<std::tuple<StableId,StableId,StableId,StableId>> unique;
    for(const auto& row:catalog.statements.rows()){
        const auto& s=row.second;if(row.first!=s.id||!id(s.id)||!authored(s.speaker_actor_id)||!skill(s.skill_id,shared)||s.terms_version!=1||(s.speaker_side!="teacher"&&s.speaker_side!="student")||(s.listener_actor_id&&(!authored(*s.listener_actor_id)||*s.listener_actor_id==s.speaker_actor_id)))return common_failure("InvalidApprenticeshipStatement",row.first);
        const auto* d=talk(interactions,s.dialogue_id);const auto* n=d?saying(*d,s.node_id):nullptr;
        const auto* role=d?shared.roles.find(d->context_role):nullptr;
        if(!d||!role||role->initial_actor!=s.speaker_actor_id||role->organization_id||!n||n->kind!="say"||n->speaker!="$provider")return common_failure("ApprenticeshipPersonalSaySourceRequired",row.first);
        // 同节点同一听者范围只能表示一份条款；泛听者与定向条目也不能重叠。
        for(const auto& other:catalog.statements.rows())if(other.first!=row.first&&other.second.speaker_actor_id==s.speaker_actor_id&&other.second.dialogue_id==s.dialogue_id&&other.second.node_id==s.node_id&&(!other.second.listener_actor_id||!s.listener_actor_id||other.second.listener_actor_id==s.listener_actor_id))return common_failure("AmbiguousApprenticeshipStatement",row.first);
        if(!unique.emplace(s.speaker_actor_id,s.dialogue_id,s.node_id,s.listener_actor_id.value_or(StableId{})).second)return common_failure("DuplicateApprenticeshipStatement",row.first);
    }
    return common_success();
}
Status validate_apprenticeship_person(const CommonStore& c,const CommonCatalog& catalog,const ApprenticeshipIdentityRead& access,const StableId& actor,Integer tick,const StableId& boundary){
    const bool historical=!boundary.empty();
    if(access.complete_people){
        const auto& people=*access.complete_people;if(!people.bound_to(c))return common_failure("FamilyContinuationCandidateMismatch",{},ErrorKind::internal);
        const auto* h=historical?c.state().facts.find(boundary):nullptr;if(historical&&!h)return common_failure("ApprenticeshipActualBoundaryRequired",boundary,ErrorKind::internal);
        auto presence=historical?people.read_presence_before_original_fact(actor,tick,{h->domain,h->kind,boundary}):people.read_current_presence(actor);
        if(!presence.ok)return {false,{},presence.error};if(!presence.value.alive)return common_failure("ApprenticeshipLivingPersonRequired",actor);
        auto age=historical?people.read_age_before_original_fact(actor,tick,{h->domain,h->kind,boundary}):people.read_current_age(actor);
        if(!age.ok)return {false,{},age.error};if(age.value.age_years<18)return common_failure("ApprenticeshipAdultRequired",actor);
        auto now=people.read_current_moment();if(!now.ok)return {false,{},now.error};if(!historical&&tick!=now.value.half_day_tick)return common_failure("ApprenticeshipCurrentTimeMismatch");
        return access.personally_available?access.personally_available(actor,tick,boundary):common_failure("ApprenticeshipAvailabilitySourceRequired",actor,ErrorKind::unsupported_capability);
    }
    auto presence=historical?read_actor_presence_before_fact(c,catalog,access.death_origin,actor,tick,boundary):read_actor_presence(c,catalog,access.death_origin,actor);
    if(!presence.ok)return {false,{},presence.error};if(!presence.value.alive)return common_failure("ApprenticeshipLivingPersonRequired",actor);
    auto age=access.births?(historical?read_actor_age_before_fact(c,catalog,*access.births,actor,tick,boundary):read_actor_age(c,catalog,*access.births,actor)):(historical?read_actor_age_before_fact(c,catalog,actor,tick,boundary):read_actor_age(c,catalog,actor));
    if(!age.ok)return {false,{},age.error};if(!age.value.adult)return common_failure("ApprenticeshipAdultRequired",actor);
    if(!historical&&tick!=c.tick())return common_failure("ApprenticeshipCurrentTimeMismatch");
    return access.personally_available?access.personally_available(actor,tick,boundary):common_failure("ApprenticeshipAvailabilitySourceRequired",actor);
}
Status validate_apprenticeship_statement_source(const CommonStore& c,const CommonCatalog& catalog,const InteractionCatalog& interactions,const ApprenticeshipCatalog& definitions,const ApprenticeshipIdentityRead& access,const ApprenticeshipStatementSource& s,const StableId& before){
    auto checked=source_binding(c,catalog,interactions,definitions,s);if(!checked.ok)return checked;
    if(before.empty()||!access.player_before)return common_failure("ApprenticeshipActualBoundaryRequired");
    auto player=access.player_before(s.tick,before);if(!player.ok)return {false,{},player.error};if(player.value!=s.listener_actor_id)return common_failure("ApprenticeshipListenerDecisionRequired");
    for(const auto* actor:{&s.speaker_actor_id,&s.listener_actor_id}){checked=validate_apprenticeship_person(c,catalog,access,*actor,s.tick,before);if(!checked.ok)return checked;}
    return common_success();
}
Outcome<ApprenticeshipStatementQuote> preview_apprenticeship_statement(const CommonStore& c,const InteractionState& meeting,const InteractionCatalog& interactions,const InteractionAccess& contact,const ApprenticeshipCatalog& definitions,const ApprenticeshipIdentityRead& access,const StableId& listener,const StableId& statement){
    auto fail=[](const Status& s)->Outcome<ApprenticeshipStatementQuote>{return {false,{},s.error};};
    if(!interactions.common)return fail(common_failure("ApprenticeshipCommonCatalogRequired"));
    const auto& catalog=*interactions.common;auto checked=validate_apprenticeship_catalog(definitions,catalog,interactions);if(!checked.ok)return fail(checked);
    const auto* d=definitions.statements.find(statement);if(!definitions.enabled||!d||listener!=c.state().metadata.player_actor_id||(d->listener_actor_id&&*d->listener_actor_id!=listener))return fail(common_failure("ApprenticeshipStatementUnavailable"));
    for(const auto* actor:{&listener,&d->speaker_actor_id}){checked=validate_apprenticeship_person(c,catalog,access,*actor,c.tick());if(!checked.ok)return fail(checked);}
    auto proof=preview_authored_node_source(c,meeting,interactions,contact,listener,d->speaker_actor_id);if(!proof.ok)return {false,{},proof.error};
    if(proof.value.dialogue_id!=d->dialogue_id||proof.value.node_id!=d->node_id||proof.value.node_kind!="say")return fail(common_failure("ApprenticeshipStatementNotCurrent"));
    ApprenticeshipStatementSource s;s.statement_id=d->id;s.speaker_actor_id=d->speaker_actor_id;s.listener_actor_id=listener;s.teacher_actor_id=d->speaker_side=="teacher"?d->speaker_actor_id:listener;s.student_actor_id=d->speaker_side=="student"?d->speaker_actor_id:listener;s.skill_id=d->skill_id;s.dialogue_id=d->dialogue_id;s.node_id=d->node_id;s.node_kind="say";s.session_id=proof.value.session_id;s.scenario_id=c.state().metadata.scenario_id;s.world_id=c.state().identity.world_id;s.context=proof.value.context;s.terms_version=d->terms_version;s.tick=c.tick();s.revision=c.state().identity.revision;
    checked=source_binding(c,catalog,interactions,definitions,s);if(!checked.ok)return fail(checked);return {true,{proof.value,std::move(s)}, {}};
}
Status revalidate_apprenticeship_statement(const CommonStore& c,const InteractionState& meeting,const InteractionCatalog& interactions,const InteractionAccess& contact,const ApprenticeshipCatalog& definitions,const ApprenticeshipIdentityRead& access,const ApprenticeshipStatementQuote& quote){
    const auto& p=quote.proof;const auto& s=quote.source;
    if(std::tie(p.actor_id,p.provider_actor_id,p.dialogue_id,p.node_id,p.node_kind,p.session_id,p.source_identity.world_id,p.source_identity.tick,p.source_identity.revision)!=std::tie(s.listener_actor_id,s.speaker_actor_id,s.dialogue_id,s.node_id,s.node_kind,s.session_id,s.world_id,s.tick,s.revision)||!same_context(p.context,s.context))return common_failure("ApprenticeshipSourceProofMismatch");
    auto checked=revalidate_authored_node_source(c,meeting,interactions,contact,p);if(!checked.ok)return checked;
    auto fresh=preview_apprenticeship_statement(c,meeting,interactions,contact,definitions,access,s.listener_actor_id,s.statement_id);if(!fresh.ok)return {false,{},fresh.error};
    return apprenticeship_source_equal(s,fresh.value.source)?common_success():common_failure("StaleApprenticeshipStatement");
}
}

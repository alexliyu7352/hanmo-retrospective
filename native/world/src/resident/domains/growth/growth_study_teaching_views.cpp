#include "growth_study_teaching_views.hpp"
#include "growth_internal.hpp"
#include "growth_equal.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
StableId hex(const WorldId& id){StableId out;const char* digits="0123456789abcdef";for(auto byte:id){out+=digits[byte>>4];out+=digits[byte&15];}return out;}
StableId person_name(const CommonStore& common,const CommonCatalog& catalog,const StableId& id){
    const auto* actor=common.state().actors.find(id);if(actor&&actor->display_name)return *actor->display_name;
    const auto* named=catalog.actor_names.find(id);const auto* value=named?catalog.texts.find(named->name_key):nullptr;return value?*value:"姓名未记";
}
V course_row(const CommonStore& common,const CommonCatalog& catalog,const StableId& action_id,const StudyFact& f){
    using namespace boundary;const auto& t=f.terms;const auto paid=(growth::original_study_lesson_cost(t.xp_before))*Integer(f.lessons.size());
    return obj({{"action_id",V{action_id}},{"student_name",V{person_name(common,catalog,t.student_id)}},{"skill_id",V{t.skill_id}},{"duration",V{t.duration}},{"completed_lessons",V{Integer(f.lessons.size())}},{"fee_paid",V{paid}},{"remaining_fee",V{t.fee-paid}},{"xp_gained",V{growth::study_xp_gained(f)}},{"paused",V{growth::study_paused(f)}},{"pause_supported",V{growth::study_pause_supported(f)}},{"phase",V{f.phase}},{"success",V{f.success}},{"code",V{f.code}},{"completed_tick",V{f.completed_tick}}});
}
}
Outcome<BoundaryValue> growth_study_teaching_view(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const SocialState& social,const GrowthAccess& access,const StableId& actor,const std::optional<InteractionContext>& context,Integer page){
    using namespace boundary;if(!catalog.study_teaching_requests.enabled)return {false,{},common_failure("StudyTeachingUnavailable").error};
    if(actor!=common.state().metadata.player_actor_id||!common.actor(actor))return {false,{},common_failure("StudyTeachingPlayerRequired").error};
    if(page<0||page>255)return {false,{},common_failure("InvalidStudyTeachingPage").error};
    V::Array offers,history;V active=obj({{"active",V{false}}});
    std::vector<const GrowthStudyTeachingRequestDefinition*> selected;
    if(context){
        if(!access.study_teaching_request)return {false,{},common_failure("StudyTeachingSourceRequired").error};
        for(const auto& row:catalog.study_teaching_requests.definitions.rows()){
            const auto& def=row.second;if(def.teacher_actor_id!=actor||def.student_actor_id!=context->provider_actor_id)continue;
            const auto source=access.study_teaching_request(actor,def.student_actor_id,def.skill_id,def.duration);
            if(!source.ok){if(source.error.kind!=ErrorKind::rule)return {false,{},source.error};continue;}
            if(source.value.agreement.request_id==def.id&&growth::context_equal(source.value.agreement.request_context,*context))selected.push_back(&def);
        }
    }
    std::sort(selected.begin(),selected.end(),[](const auto* a,const auto* b){return a->id<b->id;});
    const auto offset=std::size_t(page)*16;
    for(std::size_t i=offset;i<selected.size()&&i<offset+16;++i){const auto& def=*selected[i];GrowthCommand command;command.kind="teach_study";command.skill_id=def.skill_id;command.ticks=def.duration;command.context=context;
        const auto quote=preview_growth(common,state,catalog,shared,social,access,actor,command);
        auto row=obj({{"request_id",V{def.id}},{"student_name",V{person_name(common,shared,def.student_actor_id)}},{"skill_id",V{def.skill_id}},{"duration",V{def.duration}},{"eligible",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}},{"params",encode_growth_command(command)}});
        if(quote.ok){row.as<V::Dict>()->emplace_back("tuition",V{quote.value.cost});row.as<V::Dict>()->emplace_back("expected_xp",V{quote.value.expected_xp});}
        offers.push_back(std::move(row));
    }
    std::vector<std::pair<StableId,const StudyFact*>> completed;
    for(const auto& row:state.facts.rows())if(const auto* f=std::get_if<StudyFact>(&row.second);f&&f->terms.study_teaching_agreement&&f->terms.teacher_id==actor){
        if(f->phase=="started"){const auto* action=common.action(row.first);if(action&&action->kind=="teach_study"&&action->actor_id==actor){active=course_row(common,shared,row.first,*f);active.as<V::Dict>()->emplace_back("active",V{true});}}
        else if(f->phase=="completed")completed.emplace_back(row.first,f);
    }
    std::sort(completed.begin(),completed.end(),[](const auto& a,const auto& b){return a.second->completed_tick!=b.second->completed_tick?a.second->completed_tick>b.second->completed_tick:a.first<b.first;});
    for(std::size_t i=offset;i<completed.size()&&i<offset+16;++i)history.push_back(course_row(common,shared,completed[i].first,*completed[i].second));
    const auto& source=common.state().identity;
    return {true,obj({{"source_identity",obj({{"world_id",V{hex(source.world_id)}},{"execution_epoch",V{source.execution_epoch}},{"revision",V{source.revision}},{"tick",V{source.tick}}})},{"actor_id",V{actor}},{"page",V{page}},{"page_size",V{Integer{16}}},{"request_count",V{Integer(selected.size())}},{"requests",V{std::move(offers)}},{"active_course",std::move(active)},{"history_count",V{Integer(completed.size())}},{"history",V{std::move(history)}}}),{}};
}
}

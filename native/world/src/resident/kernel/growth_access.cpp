// 成长服务只借当前唯一候选的真实跨域来源；返回值和回调不复制人物、任务、家庭或军队权威。
#include "resident/domains/family/innate_study_source.hpp"
#include "political_takeover_execution_world.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/domains/growth/growth_study_teaching_requests.hpp"
#include "resident/domains/tasks/task_execution_gate.hpp"
namespace hanmo::resident {
GrowthAccess BundleServices::growth_access(const DynamicStepInput* phase) const {
    GrowthAccess access;
    // 返回值持有当前候选桥，回调中的身份和状态引用不会借已销毁的临时服务。
    access.personal_recovery_owner=make_growth_personal_recovery_services(world_,phase);
    access.personal_recovery=access.personal_recovery_owner.get();
    access.market_capture=market_history_.get();
    access.contact=[this](const StableId& actor,const InteractionContext& context,const StableId& service,const StableId& own_action){
        auto ia=interaction_access();
        return validate_interaction(world_.common,world_.interaction,*world_.catalog->interaction,ia,actor,context,service,own_action);
    };
    access.battle_occupied=[this](const StableId& actor){return in_open_battle(actor)||personal_occupied(world_.personal,actor);};
    access.personal_skill=[this](const StableId& id)->std::optional<GrowthPersonalSkill>{
        auto* skill=world_.catalog->personal->skills.find(id);
        if(!skill)return std::nullopt;
        return GrowthPersonalSkill{skill->kind,skill->practice_ticks};
    };
    access.skill_eligible=[this](const ActorRecord& actor,const StableId& id){
        auto* skill=world_.catalog->personal->skills.find(id);
        return skill?personal_skill_eligible(actor,*skill):common_failure("UnknownPersonalSkill");
    };
    access.armor_defense=[this](const StableId& id)->std::optional<Integer>{
        auto* value=world_.catalog->personal->armor_defenses.find(id);
        return value?std::optional<Integer>{*value}:std::nullopt;
    };
    access.procurement_authorized=[this](const StableId& actor,const StableId& task,const StableId& item){
        auto takeover=PoliticalTakeoverExecutionWorldAccess::create(world_);if(!takeover.ok)return false;auto execution=read_task_execution_context(world_.common,world_.tasks,*world_.catalog->tasks,takeover.value->tasks(),task);if(!execution.ok||!execution.value.may_progress)return false;
        return task_procurement_authorized_now(world_.common,world_.tasks,*world_.catalog->tasks,actor,task,item);
    };
    access.procurement_payers=[this](const StableId& actor){auto rows=task_procurement_payers_now(world_.common,world_.tasks,*world_.catalog->tasks,actor);auto takeover=PoliticalTakeoverExecutionWorldAccess::create(world_);if(!takeover.ok)return std::vector<std::pair<StableId,StableId>>{};rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto& payer){for(const auto& row:world_.tasks.tasks.rows())if(row.second.assignee==actor&&row.second.offer_id==payer.first&&row.second.budget_wallet_id==payer.second){auto execution=read_task_execution_context(world_.common,world_.tasks,*world_.catalog->tasks,takeover.value->tasks(),row.first);return !execution.ok||!execution.value.may_progress;}return true;}),rows.end());return rows;};
    access.task_name=[this](const StableId& task){return task_display_name(world_.tasks,*world_.catalog->tasks,task);};
    access.rest_estate=[this](const StableId& id)->std::optional<RestEstateView>{
        auto* record=world_.estate.estates.find(id);
        auto* definition=world_.catalog->estate->estates.find(id);
        if(!record||!definition)return std::nullopt;
        return RestEstateView{record->owner_actor_id,definition->place_id,definition->inventory_id,definition->food_item_id,definition->ration_per_person,record->resident_actor_ids};
    };
    access.rest_children=[this](const StableId& actor){return family_children(world_.family,actor);};
    access.rest_quiet=[this](const StableId& actor){
        for(const auto& session:world_.interaction.interactions)if(session.actor_id==actor)return false;
        return party_id(actor).empty()&&active_army_for_actor(world_.military,actor)==nullptr;
    };
    access.teaching_request=[this](const StableId& teacher,const StableId& student,const StableId& skill){return preview_growth_teaching_request(world_.common,world_.interaction,*world_.catalog->interaction,interaction_access(),world_.catalog->growth->teaching_requests,teacher,student,skill);};
    access.revalidate_teaching_request=[this](const GrowthTeachingRequestQuote& source){return revalidate_growth_teaching_request(world_.common,world_.interaction,*world_.catalog->interaction,interaction_access(),world_.catalog->growth->teaching_requests,source);};
    access.validate_teaching_agreement=[this](const GrowthTeachingAgreement& source){return validate_growth_teaching_agreement_source(world_.common.state(),*world_.catalog->common,*world_.catalog->interaction,world_.catalog->growth->teaching_requests,source);};
    access.study_teaching_request=[this](const StableId& teacher,const StableId& student,const StableId& skill,Integer duration){auto ga=growth_access();auto ia=interaction_access();GrowthStudyTeachingRead read{world_.common,world_.growth,*world_.catalog->growth,*world_.catalog->common,world_.social,ga,world_.interaction,*world_.catalog->interaction,ia};return preview_growth_study_teaching_source(read,world_.catalog->growth->study_teaching_requests,teacher,student,skill,duration);};
    access.revalidate_study_teaching_request=[this](const GrowthStudyTeachingRequestSource& source){auto ga=growth_access();auto ia=interaction_access();GrowthStudyTeachingRead read{world_.common,world_.growth,*world_.catalog->growth,*world_.catalog->common,world_.social,ga,world_.interaction,*world_.catalog->interaction,ia};return revalidate_growth_study_teaching_source(read,world_.catalog->growth->study_teaching_requests,source);};
    access.validate_study_teaching_agreement=[this](const GrowthStudyTeachingAgreement& source,const StudyTerms& terms){return validate_growth_study_teaching_agreement(world_.common,*world_.catalog->common,*world_.catalog->growth,*world_.catalog->interaction,world_.catalog->growth->study_teaching_requests,source,terms);};
    if(world_.catalog->growth->innate_learning){
        access.innate_study_source=[this](const StableId& actor,const StableId& skill)->Outcome<InnateStudyRead>{if(!world_.family_birth_enabled()||!world_.catalog->family_lineage)return {false,{},common_failure("InnateLearningProfileRequired").error};return read_actor_innate_study_source(world_.common,*world_.catalog->common,world_.family,*world_.catalog->family,*world_.catalog->family_lineage,world_.family_birth,actor,skill);};
        access.validate_innate_study_source=[this](const InnateStudySource& source,const StableId& action){if(!world_.family_birth_enabled()||!world_.catalog->family_lineage)return common_failure("InnateLearningProfileRequired");return hanmo::resident::validate_innate_study_source(world_.common,*world_.catalog->common,world_.family,*world_.catalog->family,*world_.catalog->family_lineage,world_.family_birth,source,action);};
    }
    return access;
}
} // clone重新构造BundleServices；上述回调只在拥有world的当前串行worker调用中借用。

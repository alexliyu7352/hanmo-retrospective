// 只读原已验证 StudyFact，按实际开工头判断所属师承；不修改课程或给经验补记来源。
#include "apprenticeship_courses.hpp"
#include "apprenticeship_receipts.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include <algorithm>
#include <map>
namespace hanmo::resident {
Outcome<ApprenticeshipCoursesPage> read_apprenticeship_courses(const CommonStore& c,const GrowthState& growth,const FamilyApprenticeshipState& state,const StableId& observer,const StableId& bond_id,Integer page){
    auto fail=[](const StableId& code)->Outcome<ApprenticeshipCoursesPage>{return {false,{},common_failure(code).error};};
    if(page<0||page>4095||observer!=c.state().metadata.player_actor_id)return fail("ApprenticeshipCoursesObserverRequired");auto valid=validate_apprenticeship_result_bindings(c,state);if(!valid.ok)return {false,{},valid.error};
    const auto* accepted=state.replies.find(bond_id);if(!accepted||accepted->decision!="accepted"||!accepted->confirmation)return fail("ApprenticeshipBondSourceRequired");const auto& participants=*accepted->confirmation;
    if(observer!=participants.teacher_actor_id&&observer!=participants.student_actor_id)return fail("ApprenticeshipCoursesObserverRequired");
    ApprenticeshipCoursesPage out;out.source_identity=c.state().identity;out.observer_actor_id=observer;out.bond_id=bond_id;out.teacher_actor_id=participants.teacher_actor_id;out.student_actor_id=participants.student_actor_id;out.skill_id=participants.skill_id;out.page=page;
    const auto& order=c.state().facts.order();std::map<StableId,const StudyFact*> selected;
    for(const auto& row:growth.facts.rows()){
        const auto* study=std::get_if<StudyFact>(&row.second);if(!study||study->terms.mode!="teacher"||study->terms.teacher_id!=out.teacher_actor_id||study->terms.student_id!=out.student_actor_id||study->terms.skill_id!=out.skill_id)continue;
        const auto* head=c.state().facts.find(row.first);if(!head||head->id!=row.first||head->kind!="skill_study"||head->domain!="cultivation"||std::find(order.begin(),order.end(),row.first)==order.end()||study->start_tick<0||study->start_tick>c.tick()||(study->phase!="started"&&study->phase!="completed"))return fail("ApprenticeshipOriginalStudyRequired");
        auto relationship=read_apprenticeship_bond_before_fact(c,state,bond_id,study->start_tick,row.first);if(!relationship.ok)return {false,{},relationship.error};if(!relationship.value||relationship.value->exit_fact_id)continue;selected.emplace(row.first,study);
    }
    out.total=static_cast<Integer>(selected.size());const auto first=static_cast<std::size_t>(page)*16;std::size_t index=0;
    for(auto it=order.rbegin();it!=order.rend();++it){auto found=selected.find(*it);if(found==selected.end())continue;if(index++<first)continue;if(out.rows.size()==16)break;const auto& study=*found->second;
        // 分段只计已入账的净额；旧末次结算课在未完成时不把预计值冒已得经验。
        const auto gained=growth::study_segmented(study)||study.phase=="completed"?growth::study_xp_gained(study):0;
        if(gained<0||gained>1200||study.lessons.size()>6||(study.phase=="completed"&&(study.completed_tick<study.start_tick||study.completed_tick>c.tick())))return fail("ApprenticeshipStudyOutcomeInvalid");
        out.rows.push_back({*it,study.phase,study.code,study.start_tick,study.completed_tick,static_cast<Integer>(study.lessons.size()),gained});
    }
    return {true,std::move(out),{}};
}
}

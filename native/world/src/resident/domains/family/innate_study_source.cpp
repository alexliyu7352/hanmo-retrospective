// 只读原始作者或唯一出生的先天账；课程开工以后新出生、新收养或当前XP不会倒填来源。
#include "innate_study_source.hpp"
#include "resident/domains/growth/growth_skill_api.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
Outcome<InnateStudyRead> read(const CommonStore& c,const CommonCatalog& common,const FamilyState& family,const FamilyCatalog& family_catalog,const FamilyLineageCatalog& lineage,const FamilyBirthState& births,const StableId& actor,const StableId& skill,Integer tick,const StableId& boundary){
    auto bad=[](const StableId& code)->Outcome<InnateStudyRead>{return {false,{},common_failure(code).error};};
    if(!lineage.enabled||!common.family_birth_identity_enabled||!common.extended_skills||!growth_skill_defined(skill,true)||tick<0||tick>c.tick())return bad("InnateStudyProfileRequired");
    const auto* person=c.state().actors.find(actor);if(!person)return bad("InnateStudyActorRequired");
    const auto& order=c.state().facts.order();auto before=order.end();if(!boundary.empty()){const auto* head=c.state().facts.find(boundary);before=std::find(order.begin(),order.end(),boundary);if(!head||head->id!=boundary||head->kind!="skill_study"||head->domain!="cultivation"||before==order.end())return bad("InnateStudyBoundaryRequired");}
    auto projection=read_birth_lineage_projection(c,common,lineage,births);if(!projection.ok)return {false,{},projection.error};
    InnateStudySource out;out.actor_id=actor;out.skill_id=skill;out.observed_tick=tick;
    if(std::find(common.actor_ids.begin(),common.actor_ids.end(),actor)!=common.actor_ids.end()){
        out.origin_kind="authored_lineage";
        if(const auto* levels=lineage.original_innate_levels.find(actor))out.known_levels=*levels;
    }else{
        if(!person->created_tick||*person->created_tick>tick)return bad("InnateStudyCreationRequired");
        const auto* head=c.state().facts.find(actor);const auto created=std::find(order.begin(),order.end(),actor);
        if(!head||head->kind!="actor_creation"||created==order.end()||(!boundary.empty()&&created>=before))return bad("InnateStudyCreationRequired");
        if(const auto* origin=projection.value.birth_sources.find(actor)){
            auto valid=validate_biological_actor_identity(c,common,family_catalog,lineage,births,*person);if(!valid.ok)return {false,{},valid.error};
            const auto at=std::find(order.begin(),order.end(),*origin);if(at==order.end()||at>=created||(!boundary.empty()&&at>=before))return bad("InnateStudyBirthSourceOrder");
            out.origin_kind="biological_birth";out.origin_fact_id=*origin;
            const auto* levels=projection.value.innate_levels.find(actor);if(!levels)return bad("InnateStudyBirthLevelsRequired");out.known_levels=*levels;
        }else{
            if(head->domain!="family")return bad("InnateStudyCreationRequired");
            auto valid=validate_family_identity(c,family,family_catalog,*person);if(!valid.ok)return {false,{},valid.error};
            // 旧创建/收养只有真实人身份，没有已抽样禀赋；不补一次随机也不复制其原型档位。
            out.origin_kind="legacy_creation_unknown";out.origin_fact_id=actor;
        }
    }
    auto bonus=birth_innate_learning_bonus(lineage.endowment,out.known_levels,skill);if(!bonus.ok)return {false,{},bonus.error};out.bonus_bp=bonus.value;
    return {true,{c.state().identity,std::move(out)},{}};
}
}
Outcome<InnateStudyRead> read_actor_innate_study_source(const CommonStore& c,const CommonCatalog& common,const FamilyState& family,const FamilyCatalog& family_catalog,const FamilyLineageCatalog& lineage,const FamilyBirthState& births,const StableId& actor,const StableId& skill){return read(c,common,family,family_catalog,lineage,births,actor,skill,c.tick(),{});}
Outcome<InnateStudyRead> read_actor_innate_study_source_before(const CommonStore& c,const CommonCatalog& common,const FamilyState& family,const FamilyCatalog& family_catalog,const FamilyLineageCatalog& lineage,const FamilyBirthState& births,const StableId& actor,const StableId& skill,Integer tick,const StableId& action){if(action.empty())return {false,{},common_failure("InnateStudyBoundaryRequired").error};return read(c,common,family,family_catalog,lineage,births,actor,skill,tick,action);}
Status validate_innate_study_source(const CommonStore& c,const CommonCatalog& common,const FamilyState& family,const FamilyCatalog& family_catalog,const FamilyLineageCatalog& lineage,const FamilyBirthState& births,const InnateStudySource& source,const StableId& action){auto actual=read_actor_innate_study_source_before(c,common,family,family_catalog,lineage,births,source.actor_id,source.skill_id,source.observed_tick,action);if(!actual.ok)return {false,{},actual.error};const auto& expected=actual.value.source;if(source.origin_kind!=expected.origin_kind||source.origin_fact_id!=expected.origin_fact_id||source.bonus_bp!=expected.bonus_bp||source.known_levels.rows()!=expected.known_levels.rows())return common_failure("InnateStudySourceMismatch");return common_success();}
}

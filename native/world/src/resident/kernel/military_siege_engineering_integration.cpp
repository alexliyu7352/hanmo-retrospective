// 原siege_step在课程前准备，所有同to技艺结算排除；技能只读不返回军事授权。
#include "military_siege_engineering_integration.hpp"
#include "world_bundle.hpp"
#include "resident/domains/growth/growth_siege_engineering_history.hpp"
#include "resident/domains/military/military_siege_engineering_profile.hpp"
namespace hanmo::resident {
namespace {
Status ready(const WorldBundle& w){return w.military_siege_engineering_enabled()&&w.catalog->common&&w.catalog->family&&w.catalog->growth?common_success():common_failure("MilitarySiegeEngineeringProfileRequired");}
GrowthSiegeEngineeringHistoryContext context(const WorldBundle& w){return {*w.catalog->common,*w.catalog->family,w.family,w.family_birth_enabled()?&w.family_birth:nullptr};}
Outcome<MilitarySiegeEngineeringSkillObservation> mapped(Outcome<GrowthSiegeEngineeringSkillObservation> read){if(!read.ok)return {false,{},read.error};auto& s=read.value;MilitarySiegeEngineeringSkillObservation out;out.source_identity=s.source_identity;out.actor_id=std::move(s.actor_id);out.skill_id=std::move(s.skill_id);out.before_fact_id=std::move(s.before_fact_id);out.authored_actor_id=std::move(s.authored_actor_id);out.observed_tick=s.observed_tick;out.experience=s.experience;out.level=s.level;out.causal_fact_ids=std::move(s.causal_fact_ids);return {true,std::move(out),{}};}
class Skills final:public MilitarySiegeEngineeringSkillRead {
 const WorldBundle& w_;std::optional<DynamicStepInput> phase_;
public:
 Skills(const WorldBundle& w,const DynamicStepInput* phase):w_(w),phase_(phase?std::optional<DynamicStepInput>{*phase}:std::nullopt){}
 Outcome<MilitarySiegeEngineeringSkillObservation> current_siege_engineering_skill(const StableId& actor)const override{auto r=ready(w_);if(!r.ok)return {false,{},r.error};return mapped(read_growth_siege_engineering_skill(w_.common,context(w_),w_.growth,actor,phase_?&*phase_:nullptr));}
 Outcome<MilitarySiegeEngineeringSkillObservation> siege_engineering_skill_before_preparation(const StableId& actor,const StableId& id)const override{
  auto r=ready(w_);if(!r.ok)return {false,{},r.error};const MilitarySiegeEngineeringPreparation* p=nullptr;for(const auto& f:w_.military.siege_engineering.preparations)if(f.id==id){if(p)return {false,{},common_failure("MilitarySiegeEngineeringPreparationAmbiguous",id).error};p=&f;}bool bound=false;if(p)for(const auto& u:p->units)if(u.commander_actor_id==actor)bound=true;if(!bound)return {false,{},common_failure("MilitarySiegeEngineeringCommandMismatch",actor).error};return mapped(read_growth_siege_engineering_skill_before_preparation(w_.common,context(w_),w_.growth,actor,p->to_tick,id,phase_?&*phase_:nullptr));
 }
};
struct OwnedAccess {
 Skills skills;MilitarySiegeEngineeringAccess access;
 OwnedAccess(WorldBundle& w,const DynamicStepInput* p):skills(w,p),access(w.common,w.military,*w.catalog->military,skills,p){}
};
}
std::shared_ptr<MilitarySiegeEngineeringAccess> make_military_siege_engineering_access(WorldBundle& w,const DynamicStepInput* phase){auto owned=std::make_shared<OwnedAccess>(w,phase);return {owned,&owned->access};}
Status validate_military_siege_engineering_world_structure(const WorldBundle& w){auto r=validate_military_siege_engineering_modules(w.catalog->identity.module_versions);if(!r.ok)return r;unsigned markers=0;for(const auto& m:w.catalog->identity.module_versions)if(m.first==military_siege_engineering_module&&m.second=="1")++markers;if(bool(markers)!=w.military_siege_engineering_enabled())return common_failure("MilitarySiegeEngineeringProfileMismatch");if(!w.military_siege_engineering_enabled()){if(!w.military.siege_engineering.preparations.empty()||!w.military.siege_engineering.field_order.empty()||!w.military.siege_lifecycle.facts.empty()||!w.military.siege_lifecycle.field_order.empty())return common_failure("UnexpectedMilitarySiegeEngineering");for(const auto& row:w.common.state().facts.rows())if(row.second.domain=="military"&&(row.second.kind==military_siege_engineering_fact_kind||row.first.rfind("military_siege_lifecycle:",0)==0))return common_failure("UnexpectedMilitarySiegeEngineering",row.first);return common_success();}return validate_military_siege_engineering_history(w.common,w.military,*w.catalog->military,*w.catalog->military->siege_engineering_policy,w.military.siege_engineering,w.military.siege_lifecycle,nullptr,w.population_military_enabled()?&w.population_military:nullptr,w.population_military_enabled()?w.catalog->population_military.get():nullptr);}
Status validate_military_siege_engineering_world_actor_sources(const WorldBundle& w){if(!w.military_siege_engineering_enabled())return common_success();Skills skills(w,nullptr);return validate_military_siege_engineering_history_actor_sources(w.common,w.military.siege_engineering,skills);}
}

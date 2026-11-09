// 不从当前技能补过去：薄适配只映射原Growth完整来源，exact准备tick仍先由军事事实交叉核。
#include "naval_gunnery_integration.hpp"
#include "world_bundle.hpp"
#include "resident/domains/growth/growth_naval_history.hpp"
#include "resident/domains/military/naval_gunnery_source.hpp"
namespace hanmo::resident {
namespace {
GrowthNavalHistoryContext context(const WorldBundle& w){return {*w.catalog->common,*w.catalog->family,w.family,w.family_birth_enabled()?&w.family_birth:nullptr};}
Outcome<NavalGunnerySkillObservation> mapped(Outcome<GrowthNavalSkillObservation> source){if(!source.ok)return {false,{},source.error};auto& s=source.value;return {true,{s.source_identity,std::move(s.actor_id),std::move(s.skill_id),std::move(s.before_fact_id),std::move(s.authored_actor_id),s.observed_tick,s.experience,s.level,std::move(s.causal_fact_ids)},{}};}
}
// 只有当前真实入战允许热技艺读；阶段from/to仍由Growth和S2分别校验。
Outcome<NavalGunnerySkillObservation> NavalGunneryWorldRead::current_naval_skill(const StableId& actor)const{if(!world_.naval_gunnery_enabled()||!phase_)return {false,{},common_failure("NavalGunneryCreationPhaseRequired",actor).error};return mapped(read_growth_naval_skill(world_.common,context(world_),world_.growth,actor,&*phase_));}
// exact事实不仅有Common头，还须是此场实际唯一准备载荷；不让任意naval_skill字符串充观察边界。
Outcome<NavalGunnerySkillObservation> NavalGunneryWorldRead::naval_skill_before(const StableId& actor,const StableId& source)const{
 if(!world_.naval_gunnery_enabled())return {false,{},common_failure("NavalGunneryProfileRequired",source).error};const NavalGunneryPreparation* prepared=nullptr;for(const auto& p:world_.naval.gunnery.preparations)if(p.id==source){if(prepared)return {false,{},common_failure("NavalGunnerySourceAmbiguous",source).error};prepared=&p;}if(!prepared)return {false,{},common_failure("NavalGunneryPreparationRequired",source).error};bool present=false;for(const auto& p:prepared->captains)if(p.captain_actor_id==actor)present=true;if(!present)return {false,{},common_failure("NavalGunneryCaptainSourceRequired",actor).error};return mapped(read_growth_naval_skill_before_preparation(world_.common,context(world_),world_.growth,actor,prepared->tick,source,phase_?&*phase_:nullptr));
}
Status validate_naval_gunnery_world(const WorldBundle& w){if(!w.naval_gunnery_enabled())return common_failure("NavalGunneryProfileRequired");NavalGunneryWorldRead read(w);return validate_naval_gunnery_history(w.common,w.naval.gunnery,w.naval.battle,read);}
}

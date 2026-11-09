// 早结构不借未验Growth；晚来源不递归World或Lesson总验，避免教师/学生取得互相自证。
#include "military_tactic_learning_integration.hpp"
#include "world_bundle.hpp"
#include "resident/domains/military/military_tactic_learning_profile.hpp"
#include "resident/domains/military/military_tactic_practice_codec.hpp"
#include "resident/domains/growth/growth_tactic_lesson_military.hpp"
#include "resident/domains/growth/growth_military_training_qualification.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
Status ready(const WorldBundle& w){return w.military_tactic_learning_enabled()&&w.catalog->common&&w.catalog->family&&w.catalog->growth?common_success():common_failure("MilitaryTacticLearningProfileRequired");}
// 服务自持phase值，只借稳定World引用；不保存临时context或可选状态成员的地址。
class Knowledge final:public MilitaryTacticKnowledgeRead {
 const WorldBundle& w_;std::optional<DynamicStepInput> phase_;
public:
 Knowledge(const WorldBundle& w,const DynamicStepInput* p):w_(w){if(p)phase_=*p;}
 Outcome<MilitaryTacticTeachingTarget> tactic_definition(const StableId& id)const override{auto r=ready(w_);if(!r.ok)return {false,{},r.error};return read_military_tactic_teaching_target(w_.common,*w_.catalog->military->battle_catalog.tactic_learning,id);}
 Outcome<std::optional<MilitaryTacticLearningObservation>> current_tactic(const StableId& actor,const StableId& tactic)const override{return tactic_before(actor,tactic,phase_?phase_->to_tick:w_.common.tick(),{});}
 Outcome<std::optional<MilitaryTacticLearningObservation>> tactic_before(const StableId& actor,const StableId& tactic,Integer tick,const StableId& before)const override{auto r=ready(w_);if(!r.ok)return {false,{},r.error};return read_military_learned_tactic(w_.common,*w_.catalog->common,*w_.catalog->military->battle_catalog.tactic_learning,w_.military_tactic_learning,actor,tactic,tick,before,phase_?&*phase_:nullptr);}
};
class Qualification final:public MilitaryTacticPracticeQualificationRead {
 const WorldBundle& w_;
public:
 explicit Qualification(const WorldBundle& w):w_(w){}
 Outcome<MilitaryTacticPracticeQualification> before_training(const StableId& actor,const StableId& skill,Integer tick,const StableId& action)const override{
  auto valid=ready(w_);if(!valid.ok)return {false,{},valid.error};GrowthSkillOriginContext origins{*w_.catalog->common,*w_.catalog->family,w_.family,w_.family_birth_enabled()?&w_.family_birth:nullptr};auto read=read_growth_military_training_qualification_before_start(w_.common,origins,w_.growth,actor,skill,tick,action);if(!read.ok)return {false,{},read.error};const auto& s=read.value;MilitaryTacticPracticeQualification out;out.source_identity=s.source_identity;out.actor_id=s.actor_id;out.skill_id=s.skill_id;out.before_fact_id=s.boundary_fact_id;out.observed_tick=s.observed_tick;out.minimum_experience=s.minimum_xp;out.minimum_level=s.minimum_level;out.causal_fact_ids=s.initial_source_ids;out.causal_fact_ids.insert(out.causal_fact_ids.end(),s.study_source_ids.begin(),s.study_source_ids.end());const auto& order=w_.common.state().facts.order();auto at=[&](const StableId& id){return std::find(order.begin(),order.end(),id);};std::sort(out.causal_fact_ids.begin(),out.causal_fact_ids.end(),[&](const auto& a,const auto& b){return at(a)<at(b);});out.causal_fact_ids.erase(std::unique(out.causal_fact_ids.begin(),out.causal_fact_ids.end()),out.causal_fact_ids.end());return {true,std::move(out),{}};
 }
};
class Sources final:public MilitaryTacticLearningSourceRead {
 const WorldBundle& w_;StableId hot_closed_;std::optional<DynamicStepInput> committed_;
public:
 explicit Sources(const WorldBundle& w):w_(w){}
 Sources(const WorldBundle& w,const StableId& id,const DynamicStepInput& step):w_(w),hot_closed_(id),committed_(step){}
 Outcome<MilitaryTacticTeachingCompletion> completed_tactic_teaching(const StableId& id)const override{return committed_&&id==hot_closed_?read_growth_tactic_teaching_completion_after_half_day(w_.common,w_.growth_tactic_lessons,id,*committed_):read_growth_tactic_teaching_completion(w_.common,w_.growth_tactic_lessons,id);}
 Outcome<MilitaryTacticPracticeCompletion> completed_tactic_practice(const StableId& id)const override{Qualification read(w_);return read_military_tactic_practice_completion(w_.common,w_.military,*w_.catalog->military,w_.military_tactic_learning,read,id,w_.naval_enabled()?&w_.naval:nullptr);}
};
}
std::unique_ptr<MilitaryTacticKnowledgeRead> make_military_tactic_knowledge_read(const WorldBundle& w,const DynamicStepInput* p){return std::make_unique<Knowledge>(w,p);}
Outcome<MilitaryTacticPracticeCompletion> preview_military_tactic_practice_world(const WorldBundle& w,const StableId& actor,const MilitaryTacticPracticeClaim& command){auto r=ready(w);if(!r.ok)return {false,{},r.error};Qualification read(w);return preview_military_tactic_practice(w.common,*w.catalog->common,w.military,*w.catalog->military,w.military_tactic_learning,read,actor,command,w.naval_enabled()?&w.naval:nullptr);}
Outcome<MilitaryTacticPracticeReceipt> apply_military_tactic_practice_world_in_candidate(WorldBundle& w,const StableId& actor,const MilitaryTacticPracticeClaim& command){auto r=ready(w);if(!r.ok)return {false,{},r.error};Qualification read(w);return apply_military_tactic_practice_in_candidate(w.common,*w.catalog->common,w.military,*w.catalog->military,w.military_tactic_learning,read,actor,command,w.naval_enabled()?&w.naval:nullptr);}
Outcome<MilitaryTacticLearningResult> record_military_tactic_teaching_world_in_candidate(WorldBundle& w,const StableId& id,const DynamicStepInput& committed){auto r=ready(w);if(!r.ok)return {false,{},r.error};Sources source(w,id,committed);return record_military_tactic_completion_in_candidate(w.common,*w.catalog->common,*w.catalog->military->battle_catalog.tactic_learning,w.military_tactic_learning,MilitaryTacticLearningSourceKind::teaching,id,source);}
Status validate_military_tactic_learning_world_structure(const WorldBundle& w){auto r=validate_military_tactic_learning_modules(w.catalog->identity.module_versions);if(!r.ok)return r;const bool selected=std::find(w.catalog->identity.module_versions.begin(),w.catalog->identity.module_versions.end(),std::make_pair(StableId{military_tactic_learning_module},StableId{"1"}))!=w.catalog->identity.module_versions.end();if(selected!=w.military_tactic_learning_enabled()||selected!=w.military_tactic_learning_seed_complete)return common_failure("MilitaryTacticLearningProfileMismatch");MilitaryTacticLearningCatalog disabled;return validate_military_tactic_learning_structure(w.common,*w.catalog->common,selected?*w.catalog->military->battle_catalog.tactic_learning:disabled,w.military_tactic_learning);}
Status validate_military_tactic_learning_world_sources(const WorldBundle& w){auto r=validate_military_tactic_learning_world_structure(w);if(!r.ok)return r;if(!w.military_tactic_learning_enabled()){for(const auto& h:w.common.state().history)if(h.kind=="military_tactic_practice")return common_failure("MilitaryTacticLearningProfileRequired",h.result_id);for(const auto& row:w.common.state().results.rows())if(row.second){const auto* e=row.second->details?std::get_if<MilitaryEffect>(&row.second->details->data):nullptr;if(row.second->kind=="military_tactic_practice"||(e&&(e->fact_id.rfind("military_tactic_practice:",0)==0||e->receipt_id.rfind("military_tactic:",0)==0)))return common_failure("MilitaryTacticLearningProfileRequired",row.first);}return common_success();}Sources source(w);r=validate_military_tactic_learning_sources(w.common,*w.catalog->common,*w.catalog->military->battle_catalog.tactic_learning,w.military_tactic_learning,source);if(!r.ok)return r;for(const auto& row:w.common.state().results.rows()){if(!row.second)continue;const auto& result=*row.second;const auto* e=result.details?std::get_if<MilitaryEffect>(&result.details->data):nullptr;const bool linked=e&&(e->fact_id.rfind("military_tactic_practice:",0)==0||e->receipt_id.rfind("military_tactic:",0)==0);if(result.kind!="military_tactic_practice"&&!linked)continue;if(!e)return common_failure("MilitaryTacticPracticeCachedEffectMismatch",row.first);r=military_tactic_practice_codec::validate_cached_effect(w.common,w.military_tactic_learning,row.first,result.kind,result.result_id,result.tick,*e);if(!r.ok)return r;}return validate_growth_tactic_teaching_acquisition_coverage(w.common,w.growth_tactic_lessons,[&](const StableId& id){return read_military_tactic_teaching_acquisition(w.common,*w.catalog->common,*w.catalog->military->battle_catalog.tactic_learning,w.military_tactic_learning,id);});}
}

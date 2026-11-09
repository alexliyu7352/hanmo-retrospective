// life_age_checks/1；原字段序与同一record同时驱动JSON、binary和有界projection。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "life_age.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "family_life_exact_codec_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
template<class C>Status rule(C& c,ActorFoundationRuleRef& r){c.field("id",r.id);c.field("version",r.version);c.field("catalog_digest",r.catalog_digest);return r.id.empty()||r.version<1||r.catalog_digest.empty()?c.reject("InvalidLifeAgeRuleRef"):c.status();}
template<class C>Status check(C& c,AgeCheckFact& f){
    c.field("id",f.id);c.field("actor_id",f.actor_id);family_life4_codec::world(c,f.world_id);c.field("scenario_id",f.scenario_id);
    c.object("age_basis",[&](C& v){return life_age_basis_fields(v,f.age_basis);});c.object("calendar_profile_ref",[&](C& v){return rule(v,f.calendar_profile_ref);});
    c.field("game_year_ordinal",f.game_year_ordinal);c.field("age_years",f.age_years);c.object("life_policy_ref",[&](C& v){return rule(v,f.life_policy_ref);});c.object("settings_ref",[&](C& v){return rule(v,f.settings_ref);});c.field("was_controlled_at_check",f.was_controlled_at_check);c.field("threshold_bp",f.threshold_bp);c.optional("draw",f.draw,[](C& v,Integer& value){v.field("value",value);return value<0||value>9999?v.reject("InvalidLifeAgeDraw"):v.status();});
    if(f.outcome!=AgeCheckFact::Outcome::survived&&f.outcome!=AgeCheckFact::Outcome::died&&f.outcome!=AgeCheckFact::Outcome::disabled&&f.outcome!=AgeCheckFact::Outcome::below_age)return c.reject("InvalidLifeAgeOutcome");
    StableId outcome=f.outcome==AgeCheckFact::Outcome::survived?"survived":f.outcome==AgeCheckFact::Outcome::died?"died":f.outcome==AgeCheckFact::Outcome::disabled?"disabled":"below_age";c.field("outcome",outcome);
    if(outcome!="survived"&&outcome!="died"&&outcome!="disabled"&&outcome!="below_age")return c.reject("InvalidLifeAgeOutcome");if(c.reading())f.outcome=outcome=="survived"?AgeCheckFact::Outcome::survived:outcome=="died"?AgeCheckFact::Outcome::died:outcome=="disabled"?AgeCheckFact::Outcome::disabled:AgeCheckFact::Outcome::below_age;
    c.field("from_tick",f.from_tick);c.field("to_tick",f.to_tick);
    const Integer threshold=f.age_years<70?0:f.age_years>=96?8000:300*(f.age_years-69);
    if(f.id.empty()||f.actor_id.empty()||f.scenario_id.empty()||f.game_year_ordinal<1||f.age_years<0||f.from_tick<0||f.from_tick==std::numeric_limits<Integer>::max()||f.to_tick!=f.from_tick+1||f.threshold_bp!=threshold)return c.reject("InvalidAgeCheckFact");
    const bool sampled=f.outcome==AgeCheckFact::Outcome::survived||f.outcome==AgeCheckFact::Outcome::died;
    if(sampled!=f.draw.has_value()||(sampled&&(f.age_years<70||((*f.draw<f.threshold_bp)!=(f.outcome==AgeCheckFact::Outcome::died))))||(f.outcome==AgeCheckFact::Outcome::below_age&&f.age_years>=70)||(f.outcome==AgeCheckFact::Outcome::disabled&&!f.was_controlled_at_check))return c.reject("InvalidLifeAgeOutcomeShape");
    return c.status();
}
template<class C>Status last(C& c,LifeAgeLastCheck& r){c.field("actor_id",r.actor_id);c.object("calendar_profile_ref",[&](C& v){return rule(v,r.calendar_profile_ref);});c.field("game_year_ordinal",r.game_year_ordinal);c.field("check_fact_id",r.check_fact_id);return r.actor_id.empty()||r.game_year_ordinal<1||r.check_fact_id.empty()?c.reject("InvalidLifeAgeLastCheck"):c.status();}
template<class C>Status settings(C& c,LifeAgeSettingsChange& r){c.field("id",r.id);c.field("actor_id",r.actor_id);c.object("before",[&](C& v){return rule(v,r.before);});c.object("after",[&](C& v){return rule(v,r.after);});c.object("effective_at",[&](C& v){return family_life4_codec::moment(v,r.effective_at);});return r.id.empty()||r.actor_id.empty()||actor_foundation_rule_equal(r.before,r.after)?c.reject("InvalidLifeAgeSettingsSource"):c.status();}
template<class C,class T,class F>Status table(C& c,const StableId& name,OrderedTable<T>& rows,F visit){if constexpr(std::is_same_v<C,CodecVisitor>)return common_checkpoint::table(c,name,rows,visit);else return c.table(name,rows,visit);}
template<class C>Status state(C& c,LifeAgeState& s){table(c,"checks",s.checks,check<C>);table(c,"last_age_check_year",s.last_age_check_year,last<C>);return table(c,"settings_changes",s.settings_changes,settings<C>);}
}
Status visit_life_age_checks(CodecVisitor& c,LifeAgeState& s){return state(c,s);}
Status emit_life_age_checks_projection(ProjectionWriter& sink,std::string_view name,const LifeAgeState& input){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,input,[&](auto& c,auto& value){return state(c,value);});}
Outcome<BoundaryValue> encode_life_age_checks(const LifeAgeState& input){BoundaryValue out;BoundaryProjectionWriter sink(out);auto valid=emit_life_age_checks_projection(sink,"",input);if(valid.ok)valid=sink.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<LifeAgeState> decode_life_age_checks(const BoundaryValue& input){LifeAgeState value;ActorFoundationJsonVisitor c(input);auto valid=state(c,value);if(valid.ok)valid=c.finish();return {valid.ok,std::move(value),valid.error};}
}

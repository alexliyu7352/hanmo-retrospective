#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "family_life_exact.hpp"
#include "family_life_exact_codec_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident {
namespace {
template<class C>Status record(C& c,FamilyDeathRecord& r){if(!c.reading()&&(!r.precise||r.kind!="family_death"||r.cause_kind!="military_casualty"||!r.cause_fact_id.empty()||!r.battle_id.empty()||r.death_tick!=0||r.recorded_tick!=0||!r.field_order.empty()))return c.reject("FamilyLifeProfileMismatch");if(c.reading())r.precise=FamilyDeathPayload4{};auto& d=*r.precise;c.field("death_id",d.death_id);c.field("actor_id",r.actor_id);family_life4_codec::world(c,r.world_id);c.field("scenario_id",r.scenario_id);c.object("effective_at",[&](C& v){return family_life4_codec::moment(v,d.effective_at);});c.object("recorded_at",[&](C& v){return family_life4_codec::moment(v,d.recorded_at);});c.object("cause",[&](C& v){return family_life4_codec::cause(v,d.cause);});if(d.death_id.empty()||r.actor_id.empty()||r.scenario_id.empty())return c.reject("InvalidFamilyDeath4");auto compared=compare_world_moments(d.effective_at,d.recorded_at);if(!compared.ok)return {false,{},compared.error};return compared.value<=0?c.status():c.reject("InvalidFamilyDeathTime");}
template<class C>Status state(C& c,FamilyDeathState& s){if(!s.field_order.empty())return c.reject("FamilyLife4UnexpectedFieldOrder");if constexpr(std::is_same_v<C,CodecVisitor>)return common_checkpoint::table(c,"deaths",s.deaths,record<C>);else return c.table("deaths",s.deaths,record<C>);}
}
Status visit_family_life4(CodecVisitor& c,FamilyDeathState& s){return state(c,s);}
Status emit_family_life4_projection(ProjectionWriter& sink,std::string_view name,const FamilyDeathState& input){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,input,[&](auto& c,auto& value){return state(c,value);});}
Outcome<BoundaryValue> encode_family_life4(const FamilyDeathState& input){BoundaryValue out;BoundaryProjectionWriter sink(out);auto valid=emit_family_life4_projection(sink,"",input);if(valid.ok)valid=sink.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<FamilyDeathState> decode_family_life4(const BoundaryValue& in){FamilyDeathState s;ActorFoundationJsonVisitor c(in);auto valid=state(c,s);if(valid.ok)valid=c.finish();return {valid.ok,std::move(s),valid.error};}
}

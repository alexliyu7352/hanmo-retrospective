#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "interaction_exact_encounter.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
namespace hanmo::resident {namespace {
struct Binding{StableId encounter_id;ExactEncounterOrigin origin;std::optional<WorldMoment>resolved_at;};
template<class A>Status ref(A&a,MomentOwnerSource&v){a.object("source_ref",[&](auto&c){c.field("domain",v.source_ref.domain);c.field("kind",v.source_ref.kind);return c.field("id",v.source_ref.id);});a.field("source_schema",v.source_schema);if(v.source_schema!=1||v.source_ref.domain!="travel"||v.source_ref.kind!="movement_contact"||v.source_ref.id.empty())return a.reject("InvalidExactEncounterContactSource");return a.status();}
template<class A>Status row(A&a,StableId&id,ExactEncounterOrigin&origin,std::optional<WorldMoment>&resolved){a.field("encounter_id",id);a.object("contact_source",[&](auto&c){return ref(c,origin.contact_source);});a.object("created_at",[&](auto&c){return dynamic_codec::exact_moment(c,origin.created_at);});return a.optional("resolved_at",resolved,[](auto&c,auto&at){return dynamic_codec::exact_moment(c,at);});}
template<class A>Status fields(A&a,InteractionState&s){Integer version=1;a.field("version",version);if(version!=1)return a.reject("UnsupportedExactEncounterSchema");if(!a.reading()){auto selected=[](const auto&e){return e.exact_origin.has_value();};dynamic_codec::FilteredRows values{s.encounters,selected};return a.write_sequence("encounters",values,[&](auto&c,const auto&e){return row(c,const_cast<StableId&>(e.id),const_cast<ExactEncounterOrigin&>(*e.exact_origin),const_cast<std::optional<WorldMoment>&>(e.resolved_at));});}
 std::vector<Binding>values;auto result=a.sequence("encounters",values,[](auto&c,auto&v){return row(c,v.encounter_id,v.origin,v.resolved_at);});if(!result.ok)return result;for(auto&e:s.encounters){e.exact_origin.reset();e.resolved_at.reset();}
 for(std::size_t i=0;i<values.size();++i){auto&v=values[i];for(std::size_t j=0;j<i;++j)if(values[j].encounter_id==v.encounter_id)return a.reject("DuplicateExactEncounterBinding",v.encounter_id);EncounterRecord*found=nullptr;for(auto&e:s.encounters)if(e.id==v.encounter_id){if(found)return a.reject("DuplicateOriginalEncounter",v.encounter_id);found=&e;}if(!found)return a.reject("ExactEncounterBindingMissing",v.encounter_id);found->exact_origin=std::move(v.origin);found->resolved_at=std::move(v.resolved_at);}return a.status();}
}
Status decode_exact_encounter_extension(const BoundaryValue&v,InteractionState&s){dynamic_codec::JsonArchive a(v);auto result=fields(a,s);if(result.ok)result=a.finish();return result;}
Outcome<BoundaryValue>encode_exact_encounter_extension(const InteractionState&s){return dynamic_projection::encode(s,[](auto&a,auto&v){return fields(a,v);});}
Status visit_exact_encounter_extension_checkpoint(CodecVisitor&a,InteractionState&s){return fields(a,s);}
Status emit_exact_encounter_extension_projection(ProjectionWriter&w,std::string_view key,const InteractionState&s){return dynamic_projection::emit(w,key,s,[](auto&a,auto&v){return fields(a,v);});}
}

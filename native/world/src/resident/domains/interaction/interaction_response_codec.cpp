#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "interaction_response_codec_fields.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
namespace hanmo::resident {namespace {
// The existing base decoder reads the same root by borrowing. This decoder
// reads only its explicitly selected owned roots, never copies a BoundaryValue prefix/tail.
template<class T,class F>Status read_table(dynamic_codec::JsonArchive&archive,const char*name,OrderedTable<T>&out,F fields){
 return dynamic_codec::table(archive,name,out,[&](auto&row,const StableId&key,T&value){
  if(key.empty())return row.reject("DuplicateInteractionResponseKey",key);
  return fields(row,value);
 },"DuplicateInteractionResponseKey");
}
Status root_keys(const BoundaryValue&root,Integer selected){
 const auto*fields=root.as<BoundaryValue::Dict>();if(!fields||fields->size()>CodecVisitor::max_items)return common_failure("InteractionResponseRootRequired");
 constexpr const char*names[]={"session_boundaries","response_sources","selected_response_tails","life_sources","action_confirmation_sources"};const std::size_t count=interaction_action_confirmation_schema_version(selected)?5:4;std::size_t next=0;
 for(auto at=fields->begin();at!=fields->end();++at){for(auto prior=fields->begin();prior!=at;++prior)if(prior->first==at->first)return common_failure("DuplicateInteractionResponseRoot",at->first);for(std::size_t n=0;n<5;++n)if(at->first==names[n]){if(n>=count)return common_failure("InteractionActionConfirmationExplicitVersionRequired");if(n!=next)return common_failure("InteractionResponseRootOrderMismatch",at->first);++next;}}
 return next==count?common_success():common_failure("InteractionResponseRootsMissing");
}
}
Status visit_interaction_response_roots(CodecVisitor&a,InteractionResponseSources&v,Integer selected){
 if(!interaction_response_version(selected))return a.reject("InteractionResponseVersionUnsupported");
 if(!a.reading())return interaction_response_codec::roots(a,v,selected);
 InteractionResponseSources candidate;candidate.physical_version=selected;auto status=interaction_response_codec::roots(a,candidate,selected);if(status.ok)v=std::move(candidate);return status;
}
Outcome<InteractionResponseSources> decode_interaction_response_roots(const BoundaryValue&root,Integer selected){
 dynamic_codec::JsonArchive archive(root);auto status=prepare_checkpoint_default_values<InteractionResponseSources>(archive);if(!status.ok)return {false,{},status.error};auto initial=make_initial_interaction_response_sources(selected);if(!initial.ok)return initial;status=root_keys(root,selected);if(!status.ok)return {false,{},status.error};auto&v=initial.value;
 status=read_table(archive,"session_boundaries",v.session_boundaries,[](auto&a,auto&r){return interaction_response_codec::boundary(a,r);});
 if(status.ok)status=read_table(archive,"response_sources",v.response_sources,[&](auto&a,auto&r){return interaction_response_codec::response(a,r,interaction_response_source_schema_version(selected));});
 if(status.ok)status=read_table(archive,"selected_response_tails",v.selected_response_tails,[](auto&a,auto&r){return interaction_response_codec::tail(a,r);});
 if(status.ok)status=read_table(archive,"life_sources",v.life_sources,[](auto&a,auto&r){return interaction_response_codec::life(a,r);});
 if(status.ok&&interaction_action_confirmation_schema_version(selected))status=read_table(archive,"action_confirmation_sources",v.action_confirmation_sources,[](auto&a,auto&r){return interaction_response_codec::confirmation(a,r);});
 // Only the selected response roots belong to this decoder. The base decoder
 // owns the other fields; root_keys above validates their shared original order.
 return status.ok?Outcome<InteractionResponseSources>{true,std::move(v),{}}:Outcome<InteractionResponseSources>{false,{},status.error};
}
Status emit_interaction_response_roots(ProjectionWriter&w,const InteractionResponseSources&v,Integer selected){DynamicProjectionArchive a(w);return interaction_response_codec::roots(a,const_cast<InteractionResponseSources&>(v),selected);}
Outcome<BoundaryValue> encode_interaction_response_roots(const InteractionResponseSources&v,Integer selected){return dynamic_projection::encode(v,[&](auto&a,auto&r){return interaction_response_codec::roots(a,r,selected);});}
Status emit_interaction_response_source_fields(ProjectionWriter&w,const InteractionResponseSource&v,Integer schema){DynamicProjectionArchive a(w);return interaction_response_codec::response(a,const_cast<InteractionResponseSource&>(v),schema);}
Status emit_interaction_action_confirmation_fields(ProjectionWriter&w,const InteractionActionConfirmationSource&v){DynamicProjectionArchive a(w);return interaction_response_codec::confirmation(a,const_cast<InteractionActionConfirmationSource&>(v));}
}

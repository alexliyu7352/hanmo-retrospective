#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "obligation_sources_codec.hpp"
#include "obligation_sources_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status initialize_economic_obligation_sources(EconomicObligationSourcesState&s,Integer selected){
 if((selected!=1&&selected!=2)||s.schema_version!=1)return common_failure("UnsupportedEconomicObligationSourcesVersion",{},ErrorKind::unsupported_capability);
 if(!s.obligation_sources.empty()||!s.payable_sources.empty()||!s.adjustments.empty()||!s.recovery_bindings.empty()||!s.responsibility_epochs.empty()||!s.payment_responsibility_bindings.empty()||!s.legacy_responsibility_prefixes.empty())return common_failure("EconomicObligationSourcesFreshStateRequired",{},ErrorKind::internal);
 s.schema_version=selected;return common_success();
}
Outcome<EconomicObligationSourcesState>decode_economic_obligation_sources(const BoundaryValue&input){return exact_fields::guarded<EconomicObligationSourcesState>([&]{ActorFoundationJsonVisitor fields(input);exact_fields::take(fields.prepare_default_values<EconomicObligationSourcesState>());EconomicObligationSourcesState out;exact_fields::take(obligation_source_fields::record(fields,out));exact_fields::take(fields.finish());return out;});}
Status emit_economic_obligation_sources_fields(ProjectionWriter&w,const EconomicObligationSourcesState&s){ActorFoundationProjectionArchive fields(w);auto result=exact_fields::guarded<Done>([&]{exact_fields::take(obligation_source_fields::record(fields,const_cast<EconomicObligationSourcesState&>(s)));return Done{};});return result.ok?w.status():Status{false,{},std::move(result.error)};}
Status emit_economic_obligation_sources(ProjectionWriter&w,std::string_view name,const EconomicObligationSourcesState&s){try{return emit_projection_record<ActorFoundationProjectionArchive>(w,name,s,[](auto&out,auto&v){return obligation_source_fields::record(out,v);});}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}}
Outcome<BoundaryValue>encode_economic_obligation_sources(const EconomicObligationSourcesState&s){return exact_fields::guarded<BoundaryValue>([&]{BoundaryValue value;BoundaryProjectionWriter out(value);exact_fields::take(emit_economic_obligation_sources(out,"",s));exact_fields::take(out.finish());return value;});}
Status visit_economic_obligation_sources(CodecVisitor&c,EconomicObligationSourcesState&s){return exact_fields::visit_binary(c,s,[](auto&out,auto&v){return obligation_source_fields::record(out,v);});}
}

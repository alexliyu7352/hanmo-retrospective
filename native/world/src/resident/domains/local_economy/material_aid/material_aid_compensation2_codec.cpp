#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "material_aid_compensation2_codec.hpp"
#include "material_aid_compensation2_fields.hpp"
#include "material_aid_codec_archive.hpp"
namespace hanmo::resident {
Status validate_treaty_material_aid_values2(const TreatyMaterialAidState2&s){
 if(s.schema_version!=2)return common_failure("UnsupportedTreatyMaterialAidVersion");
 auto valid=validate_treaty_material_aid_shape(s.principal);if(!valid.ok)return valid;
 return validate_material_aid_compensation_values2(s.compensation);
}
Status initialize_treaty_material_aid2(TreatyMaterialAidState2&s){
 auto valid=validate_treaty_material_aid_values2(s);if(!valid.ok)return valid;
 const auto&p=s.principal;if(!p.schedule_commitments.rows().empty()||!p.occurrence_cap_reservations.rows().empty()||!p.establishments.rows().empty()||!p.obligations.rows().empty()||!p.resource_issues.rows().empty()||!p.receipts.rows().empty()||!p.schedule_releases.rows().empty()||!p.deadline_results.rows().empty()||!s.compensation.scopes.rows().empty()||!s.compensation.holds.rows().empty()||!s.compensation.dispositions.rows().empty()||!s.compensation.payments.rows().empty())return common_failure("MaterialAidFreshSelectedRootRequired");
 return common_success();
}
Outcome<TreatyMaterialAidState2>decode_treaty_material_aid2(const BoundaryValue&input){try{TreatyMaterialAidState2 out;material_aid_codec::decode(input,out,"/treaty_material_aid2");auto valid=validate_treaty_material_aid_values2(out);if(!valid.ok)return {false,{},valid.error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}}
Status emit_treaty_material_aid_projection2(ProjectionWriter&w,const StableId&name,const TreatyMaterialAidState2&s){auto valid=validate_treaty_material_aid_values2(s);if(!valid.ok)return w.reject(valid.error.original.code,valid.error.original.path);try{return material_aid_codec::emit(w,name,s);}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}}
Outcome<BoundaryValue>encode_treaty_material_aid2(const TreatyMaterialAidState2&s){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=emit_treaty_material_aid_projection2(writer,"",s);if(status.ok)status=writer.finish();if(!status.ok)return {false,{},status.error};return {true,std::move(out),{}};}
Status visit_treaty_material_aid_checkpoint2(CodecVisitor&c,TreatyMaterialAidState2&s){try{if(!c.reading()){auto valid=validate_treaty_material_aid_values2(s);if(!valid.ok)return c.reject(valid.error.original.code,valid.error.original.path);}auto result=material_aid_codec::checkpoint(c,"treaty_material_aid2",s);if(!result.ok)return result;auto valid=validate_treaty_material_aid_values2(s);if(!valid.ok)return c.reject(valid.error.original.code,valid.error.original.path);return c.status();}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}}
Status emit_material_aid_compensation_fact_payload2(ProjectionWriter&w,const StableId&name,const CommonStore&c,const TreatyMaterialAidState2&s,const FactRecord&header){
 const auto*actual=c.state().facts.find(header.id);if(s.schema_version!=2||!actual||actual->domain!=header.domain||actual->kind!=header.kind||header.domain!="local_economy")return w.reject("AidCompensationOriginalHeaderRequired",header.id);
 try{if(header.kind=="aid_compensation_accepted"){const auto*r=s.compensation.scopes.find(header.id);if(!r)return w.reject("AidCompensationOriginalPayloadRequired",header.id);return material_aid_codec::emit(w,name,*r);}if(header.kind=="aid_compensation_disposed"){const auto*r=s.compensation.dispositions.find(header.id);if(!r)return w.reject("AidCompensationOriginalPayloadRequired",header.id);return material_aid_codec::emit(w,name,*r);}return emit_material_aid_original_fact_payload(w,name,c,s.principal,header);}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}
}
}

// 地方经济目录和冷状态逐字段转换；缺字段、未知枚举或重复键都明确失败。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "local_economy_codec_detail.hpp"
#include "born_population_sources.hpp"
namespace hanmo::resident {
Outcome<LocalEconomyCatalog> decode_local_economy_catalog(const BoundaryValue& root,bool military_adapter,bool named_membership,bool production_management,bool household_provisioning) {
    try {const auto* section=root.find("local_economy");auto out=dynamic_economy_codec::decode_root<LocalEconomyCatalog>(section?*section:root,"/local_economy",military_adapter,named_membership,production_management,household_provisioning);return {true,std::move(out),{}};}
    catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
}
Outcome<LocalEconomyState> decode_local_economy_state(const BoundaryValue& root,bool military_adapter,bool named_membership,bool production_management,bool household_provisioning) {
    try {auto out=dynamic_economy_codec::decode_root<LocalEconomyState>(root,"/local_economy",military_adapter,named_membership,production_management,household_provisioning);return {true,std::move(out),{}};}
    catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
}
namespace {
Status emit_selected_state_fields(ProjectionWriter& writer,const LocalEconomyState& state,bool military_adapter,bool named_membership,bool production_management,bool household_provisioning,bool born_population){try{if(!household_provisioning&&state.household_provisioning)boundary::fail("HouseholdProvisionCodecRequired","/local_economy");if(!production_management&&(state.production_management||std::any_of(state.production_receipts.rows().begin(),state.production_receipts.rows().end(),[](const auto& row){return row.second.production_context.has_value();})))boundary::fail("ProductionManagementCodecRequired","/local_economy");if(!born_population&&!state.named_birth_population_transitions.empty())boundary::fail("NamedBirthPopulationSelectedSourceRequired","/local_economy");return dynamic_economy_codec::emit_fields(writer,state,military_adapter,named_membership,production_management,household_provisioning,born_population);}catch(const boundary::DecodeFailure&failure){return writer.reject(failure.error.original.code,failure.error.original.path);}}
}
Status emit_local_economy_state_fields(ProjectionWriter& writer,const LocalEconomyState& state,bool military_adapter,bool named_membership,bool production_management,bool household_provisioning){return emit_selected_state_fields(writer,state,military_adapter,named_membership,production_management,household_provisioning,false);}

BoundaryValue encode_local_economy_state(const LocalEconomyState& state,bool military_adapter,bool named_membership,bool production_management,bool household_provisioning){BoundaryValue out;BoundaryProjectionWriter writer(out);auto valid=writer.object("",[&](auto&fields){return emit_local_economy_state_fields(fields,state,military_adapter,named_membership,production_management,household_provisioning);});if(valid.ok)valid=writer.finish();if(!valid.ok)throw boundary::DecodeFailure{valid.error};return out;}
namespace {
Status codec_selection_failure(const char* code){
 if(ScopedCheckpointJsonDecode::current()||ScopedCheckpointBoundaryDecode::current())checkpoint_decode_boundary_failure(code,{"local_economy"},ErrorKind::rule);
 return common_failure(code,"local_economy");
}
}
Status validate_local_economy_codec_selection(Integer selected_version,bool military_adapter,bool named_membership,bool production_management,bool household_provisioning){
 const Integer expected_base=household_provisioning?5:production_management?4:named_membership?3:military_adapter?2:1;
 Integer actual_base=0;
 switch(named_birth_population_selected(selected_version)?selected_version-10:selected_version){
 case 1:case 6:actual_base=1;break;
 case 2:case 7:actual_base=2;break;
 case 3:case 8:actual_base=3;break;
 case 4:case 9:actual_base=4;break;
 case 5:case 10:actual_base=5;break;
 default:return codec_selection_failure("UnsupportedLocalEconomyCodecVersion");
 }
 if(actual_base!=expected_base)return codec_selection_failure("LocalEconomyCodecSelectionMismatch");
 return common_success();
}
Outcome<LocalEconomyCatalog> decode_local_economy_catalog(const BoundaryValue&input,bool military,bool named,bool management,bool household,Integer selected){
 auto valid=validate_local_economy_codec_selection(selected,military,named,management,household);if(!valid.ok)return {false,{},valid.error};
 return decode_local_economy_catalog(input,military,named,management,household);
}
Outcome<LocalEconomyState> decode_local_economy_state(const BoundaryValue&input,bool military,bool named,bool management,bool household,Integer selected){
 auto valid=validate_local_economy_codec_selection(selected,military,named,management,household);if(!valid.ok)return {false,{},valid.error};
 try{auto out=dynamic_economy_codec::decode_root<LocalEconomyState>(input,"/local_economy",military,named,management,household,named_birth_population_selected(selected));return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
}
Status emit_local_economy_state_fields(ProjectionWriter&writer,const LocalEconomyState&state,bool military,bool named,bool management,bool household,Integer selected){
 auto valid=validate_local_economy_codec_selection(selected,military,named,management,household);if(!valid.ok)return writer.reject(valid.error.original.code,valid.error.original.path);
 return emit_selected_state_fields(writer,state,military,named,management,household,named_birth_population_selected(selected));
}
BoundaryValue encode_local_economy_state(const LocalEconomyState&state,bool military,bool named,bool management,bool household,Integer selected){
 auto valid=validate_local_economy_codec_selection(selected,military,named,management,household);if(!valid.ok)throw boundary::DecodeFailure{valid.error};
 BoundaryValue out;BoundaryProjectionWriter writer(out);valid=writer.object("",[&](auto& fields){return emit_local_economy_state_fields(fields,state,military,named,management,household,selected);});if(valid.ok)valid=writer.finish();if(!valid.ok)throw boundary::DecodeFailure{valid.error};return out;
}
} // 解码不执行义务、分配 ID 或恢复期补偿。

#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "provision_management_codec.hpp"
#include "../local_economy_codec_detail.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident::dynamic_economy_codec {
// Proof fields are embedded explicitly, leaving the Family owner's codec namespace untouched.
template<class A> void provision_authority(A& a,HouseholdProvisionControlProof& proof){
    a("authority_actor_id",proof.actor_id);a("authority_household_id",proof.household_group_id);a("authority_fact_id",proof.fact_id);a("authority_role_id",proof.source_role_id);a("authority_operation",proof.operation);a("authority_revision",proof.source_revision);a("authority_from_tick",proof.effective_from_tick);a("authority_through_tick",proof.through_tick);
}
template<class A> void record(A& a,HouseholdProvisionManagementCatalog& value){a("version",value.version);a("authorization_ids",value.authorization_ids);}
template<class A> void record(A& a,HouseholdProvisionConfiguration& value){
    a("id",value.id);a("authorization_id",value.authorization_id);a("actor_id",value.actor_id);a("previous_configuration_id",value.previous_configuration_id);provision_authority(a,value.authority);a("requested_tick",value.requested_tick);a("requested_revision",value.requested_revision);a("effective_tick",value.effective_tick);a("first_due_tick",value.first_due_tick);a("consumption_limit",value.consumption_limit);
}
template<class A> void record(A& a,HouseholdProvisionConfigurationResolution& value){
    a("id",value.id);a("configuration_id",value.configuration_id);a("kind",value.kind);a("actor_id",value.actor_id);provision_authority(a,value.authority);a("tick",value.tick);a("source_revision",value.source_revision);
}
template<class A> void record(A& a,HouseholdProvisionManagementState& value){a("version",value.version);a("configurations",value.configurations);a("resolutions",value.resolutions);a("current_by_authorization",value.current_by_authorization);}
}
namespace hanmo::resident {
namespace {
template<class T> Outcome<T> decode_management(const BoundaryValue& value){
    try{auto out=dynamic_economy_codec::decode_root<T>(value,"/household_provision_management");if(out.version!=1)boundary::unsupported("HouseholdProvisionManagementVersionMismatch","/version");return {true,std::move(out),{}};}
    catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
}
}
Outcome<HouseholdProvisionManagementCatalog> decode_household_provision_management_catalog(const BoundaryValue& value){return decode_management<HouseholdProvisionManagementCatalog>(value);}
BoundaryValue encode_household_provision_management_catalog(const HouseholdProvisionManagementCatalog& value){if(value.version!=1)boundary::unsupported("HouseholdProvisionManagementVersionMismatch","/version");return dynamic_economy_codec::encode(value);}
Outcome<HouseholdProvisionManagementState> decode_household_provision_management_state(const BoundaryValue& value){return decode_management<HouseholdProvisionManagementState>(value);}
Status emit_household_provision_management_state_fields(ProjectionWriter& writer,const HouseholdProvisionManagementState& value){try{if(value.version!=1)boundary::unsupported("HouseholdProvisionManagementVersionMismatch","/version");return dynamic_economy_codec::emit_fields(writer,value);}catch(const boundary::DecodeFailure&failure){return writer.reject(failure.error.original.code,failure.error.original.path);}}
BoundaryValue encode_household_provision_management_state(const HouseholdProvisionManagementState& value){BoundaryValue out;BoundaryProjectionWriter writer(out);auto valid=writer.object("",[&](auto&fields){return emit_household_provision_management_state_fields(fields,value);});if(valid.ok)valid=writer.finish();if(!valid.ok)throw boundary::DecodeFailure{valid.error};return out;}
Status visit_household_provision_management_checkpoint(CodecVisitor& visitor,HouseholdProvisionManagementState& value){
    if(!visitor.reading()&&value.version!=1)return visitor.reject("HouseholdProvisionManagementVersionMismatch");
    auto status=dynamic_economy_codec::checkpoint(visitor,"household_provision_management",value);if(!status.ok)return status;
    if(value.version!=1)return visitor.reject("HouseholdProvisionManagementVersionMismatch");return visitor.status();
}
Outcome<ConfigureHouseholdProvisionCommand> decode_configure_household_provision(const BoundaryValue& value){
    try{
        boundary::fields(value,{"actor_id","authorization_id","kind","consumption_limit"},{},"/configure_household_provision");ConfigureHouseholdProvisionCommand command;
        command.actor_id=boundary::get_string(value,"actor_id","");command.authorization_id=boundary::get_string(value,"authorization_id","");
        const auto kind=boundary::get_string(value,"kind","");
        if(kind=="configure")command.kind=HouseholdProvisionConfigurationKind::configure;else if(kind=="cancel_pending")command.kind=HouseholdProvisionConfigurationKind::cancel_pending;else boundary::fail("InvalidHouseholdProvisionConfigurationKind","/kind");
        dynamic_economy_codec::decode(boundary::field(value,"consumption_limit",""),command.consumption_limit,"/consumption_limit");
        if(command.actor_id.empty()||command.authorization_id.empty()||(command.consumption_limit&&*command.consumption_limit<0)||(command.kind==HouseholdProvisionConfigurationKind::cancel_pending&&command.consumption_limit))boundary::fail("InvalidHouseholdProvisionCommand","");
        return {true,std::move(command),{}};
    }catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
}
BoundaryValue encode_configure_household_provision(const ConfigureHouseholdProvisionCommand& command){
    using V=BoundaryValue;return boundary::obj({{"actor_id",V{command.actor_id}},{"authorization_id",V{command.authorization_id}},{"kind",V{command.kind==HouseholdProvisionConfigurationKind::configure?"configure":"cancel_pending"}},{"consumption_limit",command.consumption_limit?V{*command.consumption_limit}:V{}}});
}
BoundaryValue encode_household_provision_configuration_quote(const HouseholdProvisionConfigurationQuote& quote){
    using V=BoundaryValue;return boundary::obj({{"authorization_id",V{quote.command.authorization_id}},{"kind",V{quote.command.kind==HouseholdProvisionConfigurationKind::configure?"configure":"cancel_pending"}},{"consumption_limit",quote.command.consumption_limit?V{*quote.command.consumption_limit}:V{}},{"effective_tick",V{quote.effective_tick}},{"first_due_tick",V{quote.first_due_tick}},{"previous_configuration_id",V{quote.previous_configuration_id}},{"pending_configuration_id",V{quote.pending_configuration_id}}});
}
// 同一字段路径供原JSON编码与checkpoint形状核验使用。
Status emit_household_provision_management_effects_fields(ProjectionWriter& w,const HouseholdProvisionManagementEffects& value){w.sequence("fact_ids",value.fact_ids,[](auto& child,const auto& id){return child.field("",id);});return w.status();}
BoundaryValue encode_household_provision_management_effects(const HouseholdProvisionManagementEffects& value){BoundaryValue result;BoundaryProjectionWriter writer(result);auto status=writer.object("",[&](auto& fields){return emit_household_provision_management_effects_fields(fields,value);});if(status.ok)status=writer.finish();if(!status.ok)boundary::fail(status.error.original.code,status.error.original.path);return result;}
Outcome<HouseholdProvisionManagementEffects> decode_household_provision_management_effects(const BoundaryValue& value){
    try{boundary::fields(value,{"fact_ids"},{},"/household_provision_configuration");HouseholdProvisionManagementEffects effects;effects.fact_ids=boundary::strings(boundary::field(value,"fact_ids",""),"/fact_ids");if(effects.fact_ids.empty()||effects.fact_ids.size()>2||(effects.fact_ids.size()==2&&effects.fact_ids[0]==effects.fact_ids[1]))boundary::fail("InvalidHouseholdProvisionReceipt","/fact_ids");return {true,std::move(effects),{}};}
    catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
}
Status visit_household_provision_management_effects(CodecVisitor& visitor,HouseholdProvisionManagementEffects& effects){
    auto status=common_checkpoint::strings(visitor,"fact_ids",effects.fact_ids);if(!status.ok)return status;if(effects.fact_ids.empty()||effects.fact_ids.size()>2||(effects.fact_ids.size()==2&&effects.fact_ids[0]==effects.fact_ids[1]))return visitor.reject("InvalidHouseholdProvisionReceipt");return visitor.status();
}
}

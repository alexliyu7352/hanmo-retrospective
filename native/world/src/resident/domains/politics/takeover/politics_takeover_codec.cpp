#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 责任账JSON与二进制使用同一严格字段序；未知版本或夹带字段不被默默接受。
#include "politics_takeover_catalog.hpp"
#include "politics_takeover_command.hpp"
#include "politics_takeover_payload_guard.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "politics_takeover_codec_fields.hpp"

namespace hanmo::resident {
Outcome<PoliticalTakeoverDecisionCommand> decode_political_takeover_command_params(const BoundaryValue& params){auto guarded=validate_political_takeover_payload(params);if(!guarded.ok)return {false,{},guarded.error};try{
    using namespace boundary;const StableId path="/political_takeover_command";fields(params,{"authority","registration_fact_id","expected_previous_approval_fact_id","decisions"},{},path);
    PoliticalTakeoverDecisionCommand command;dynamic_economy_codec::decode(field(params,"authority",path),command.authority,path+"/authority");command.registration_fact_id=get_string(params,"registration_fact_id",path);dynamic_economy_codec::decode(field(params,"expected_previous_approval_fact_id",path),command.expected_previous_approval_fact_id,path+"/expected_previous_approval_fact_id");auto decisions=field(params,"decisions",path);array(decisions,path+"/decisions");std::vector<std::optional<StableId>> supply;
    for(auto& value:*decisions.as<BoundaryValue::Array>()){std::optional<StableId> chosen;if(const auto* policy=value.find("supply_policy_fact_id")){chosen=string(*policy,path+"/decisions/supply_policy_fact_id");if(chosen->empty())fail("InvalidSupplyPolicyReference",path);auto* fields=value.as<BoundaryValue::Dict>();fields->erase(std::remove_if(fields->begin(),fields->end(),[](const auto& field){return field.first=="supply_policy_fact_id";}),fields->end());}supply.push_back(chosen);}
    dynamic_economy_codec::decode(decisions,command.decisions,path+"/decisions");for(std::size_t i=0;i<supply.size();++i)command.decisions[i].supply_policy_fact_id=std::move(supply[i]);
    return {true,std::move(command),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<PoliticalTakeoverDecisionCommand> decode_political_takeover_command(const BoundaryValue& params,const StableId& actor,const WorldIdentity& identity){
    auto command=decode_political_takeover_command_params(params);if(!command.ok)return command;
    if(actor!=command.value.authority.actor_id)return {false,{},common_failure("PoliticalTakeoverActorMismatch").error};
    command.value.expected_identity=identity;return command;
}
BoundaryValue encode_political_takeover_command_params(const PoliticalTakeoverDecisionCommand& command){auto decisions=dynamic_economy_codec::encode(command.decisions);for(std::size_t i=0;i<command.decisions.size();++i)if(command.decisions[i].supply_policy_fact_id)(*decisions.as<BoundaryValue::Array>())[i].as<BoundaryValue::Dict>()->emplace_back("supply_policy_fact_id",BoundaryValue{*command.decisions[i].supply_policy_fact_id});return boundary::obj({{"authority",dynamic_economy_codec::encode(command.authority)},{"registration_fact_id",BoundaryValue{command.registration_fact_id}},{"expected_previous_approval_fact_id",dynamic_economy_codec::encode(command.expected_previous_approval_fact_id)},{"decisions",std::move(decisions)}});}
// 报价仅向界面暴露当前已核参数与零耗时，不包含可变World或实际账户余额。
BoundaryValue encode_political_takeover_quote(const PoliticalTakeoverQuote& quote){return boundary::obj({{"source_identity",dynamic_economy_codec::encode(quote.source_identity)},{"actor_id",BoundaryValue{quote.actor_id}},{"grant_id",BoundaryValue{quote.grant_id}},{"duration",BoundaryValue{Integer{0}}},{"command",encode_political_takeover_command_params(quote.command)}});}
// 原目录条件尾跟随原record发射；默认版本仍省略，不构造中间字段树。
Status emit_political_takeover_catalog_fields(ProjectionWriter& writer,const PoliticalTakeoverCatalog& s){
 try{if(s.version!=1)boundary::fail("UnsupportedPoliticalTakeoverVersion",{});auto status=dynamic_economy_codec::emit_fields(writer,s);if(!status.ok)return status;if(s.task_execution_version!=1)return writer.field("task_execution_version",s.task_execution_version);return writer.status();}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_takeover_catalog(const PoliticalTakeoverCatalog& s){return politics_projection_detail::encode_fields(s,emit_political_takeover_catalog_fields);}
Outcome<PoliticalTakeoverCatalog> decode_political_takeover_catalog(const BoundaryValue& v){auto guarded=validate_political_takeover_payload(v);if(!guarded.ok)return {false,{},guarded.error};try{PoliticalTakeoverCatalog s;auto original=v;if(const auto* version=v.find("task_execution_version")){s.task_execution_version=boundary::integer(*version,"/political_takeover/task_execution_version");auto* fields=original.as<BoundaryValue::Dict>();if(fields)fields->erase(std::remove_if(fields->begin(),fields->end(),[](const auto& row){return row.first=="task_execution_version";}),fields->end());}dynamic_economy_codec::decode(original,s,"/political_takeover");if(s.version!=1||(s.task_execution_version!=1&&s.task_execution_version!=2)||(!s.enabled&&s.task_execution_version!=1))return {false,{},common_failure("UnsupportedPoliticalTakeoverVersion").error};return {true,std::move(s),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status emit_political_takeover_state_fields(ProjectionWriter& writer,const PoliticalTakeoverState& s){
 try{if(s.version!=1)boundary::fail("UnsupportedPoliticalTakeoverVersion",{});for(const auto& r:s.registrations.rows())if(r.second.registered_at)boundary::fail("UnexpectedPoliticalTakeoverMoment",r.first);for(const auto& r:s.approvals.rows())if(r.second.approved_at)boundary::fail("UnexpectedPoliticalTakeoverMoment",r.first);return dynamic_economy_codec::emit_fields(writer,s);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_takeover_state(const PoliticalTakeoverState& s){return politics_projection_detail::encode_fields(s,emit_political_takeover_state_fields);}
BoundaryValue encode_political_original_responsibility(const PoliticalOriginalResponsibility& s){return dynamic_economy_codec::encode(s);}
BoundaryValue encode_political_responsibility_decision(const PoliticalResponsibilityDecision& s){return dynamic_economy_codec::encode(s);}
Outcome<PoliticalTakeoverState> decode_political_takeover_state(const BoundaryValue& v){
    auto guarded=validate_political_takeover_payload(v);if(!guarded.ok)return {false,{},guarded.error};
    try{PoliticalTakeoverState s;dynamic_economy_codec::decode(v,s,"/political_takeover");if(s.version!=1)return {false,{},common_failure("UnsupportedPoliticalTakeoverVersion").error};return {true,std::move(s),{}};}
    catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
Status visit_political_takeover_checkpoint(CodecVisitor& c,PoliticalTakeoverState& s){
    try{if(!c.reading()){if(s.version!=1)return c.reject("UnsupportedPoliticalTakeoverVersion",{});for(const auto& r:s.registrations.rows())if(r.second.registered_at)return c.reject("UnexpectedPoliticalTakeoverMoment",r.first);for(const auto& r:s.approvals.rows())if(r.second.approved_at)return c.reject("UnexpectedPoliticalTakeoverMoment",r.first);}auto r=dynamic_economy_codec::checkpoint(c,"political_takeover",s);if(!r.ok)return r;if(s.version!=1)return c.reject("UnsupportedPoliticalTakeoverVersion",{});return c.status();}
    catch(const boundary::DecodeFailure&){return c.reject("InvalidPoliticalTakeoverCheckpoint",{});}
}
}

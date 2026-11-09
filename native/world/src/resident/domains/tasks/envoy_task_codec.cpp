// 使者合同独立typed冷形状；解码不是政治授权、人物知情或任务履约验证。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "envoy_task_state.hpp"
#include "task_dynamic_codec.hpp"
namespace hanmo::resident {
namespace {
// 本模块显式编码原typed上下文；解码错误不能写进持久行动，字段顺序只为冷往返保留。
template<class A>Status context(A& c,InteractionContext& v){if(v.input_error)return c.reject("InvalidInteractionContext");c.field("place_id",v.place_id);c.field("provider_actor_id",v.provider_actor_id);c.field("role_id",v.role_id);c.optional("kind",v.kind,[](auto& row,auto& value){return row.field("value",value);});c.optional("encounter_id",v.encounter_id,[](auto& row,auto& value){return row.field("value",value);});return dynamic_codec::strings(c,"field_order",v.field_order);}
// 原批准的两端Office/site及提议版本逐项保存，不从当前远方人物位置补字段。
template<class A>Status acceptance(A& c,EnvoyTaskAcceptance& a){
    c.field("fact_id",a.fact_id);c.field("task_id",a.task_id);c.field("contract_id",a.contract_id);c.field("mission_authority_fact_id",a.mission_authority_fact_id);c.field("proposal_id",a.proposal_id);c.field("proposal_fact_id",a.proposal_fact_id);c.field("terms_revision",a.terms_revision);c.field("issuer_organization_id",a.issuer_organization_id);c.field("target_organization_id",a.target_organization_id);c.field("envoy_actor_id",a.envoy_actor_id);c.field("target_office_id",a.target_office_id);c.field("delivery_site_id",a.delivery_site_id);c.field("report_office_id",a.report_office_id);c.field("report_site_id",a.report_site_id);c.field("continuity_policy_id",a.continuity_policy_id);c.field("display_name",a.display_name);c.field("accepted_revision",a.accepted_revision);c.field("accepted_tick",a.accepted_tick);c.field("delivery_expires_tick",a.delivery_expires_tick);return c.field("return_report_required",a.return_report_required);
}
// 当前交接只持Common Action引用与原上下文，既不恢复报价权限也不复制倒计时。
template<class A>Status action(A& c,EnvoyTaskAction& a){c.field("action_id",a.action_id);dynamic_codec::enumeration(c,"kind",a.kind,1);return c.object("context",[&](auto& row){return context(row,a.context);});}
// 任务交付事实仅链接政治唯一presentation与原本人的真实交接Action。
template<class A>Status delivery(A& c,EnvoyTaskDelivery& d){c.field("fact_id",d.fact_id);c.field("task_id",d.task_id);c.field("contract_id",d.contract_id);c.field("action_id",d.action_id);c.field("acceptance_fact_id",d.acceptance_fact_id);c.field("diplomatic_presentation_fact_id",d.diplomatic_presentation_fact_id);return c.field("completed_tick",d.completed_tick);}
// 任务终结与政治提议接受分离；结果枚举、可选递交/返报引用严格按版本读取。
template<class A>Status completion(A& c,EnvoyTaskCompletion& f,Integer version){c.field("fact_id",f.fact_id);c.field("task_id",f.task_id);c.field("contract_id",f.contract_id);c.field("acceptance_fact_id",f.acceptance_fact_id);c.optional("delivery_fact_id",f.delivery_fact_id,[](auto& row,auto& value){return row.field("value",value);});c.optional("report_action_id",f.report_action_id,[](auto& row,auto& value){return row.field("value",value);});dynamic_codec::enumeration(c,"result",f.result,2);c.field("completed_tick",f.completed_tick);auto status=c.field("reason",f.reason);if(!status.ok)return status;if(version==2)return c.optional("diplomatic_report_fact_id",f.diplomatic_report_fact_id,[](auto& row,auto& value){return row.field("value",value);});return status;}
// 中止记录只保存已经发生的工时/接触对象；不恢复原行动，也不创造第二份剩余时间。
template<class A>Status interruption(A& c,EnvoyHandoverInterruption& f){c.field("fact_id",f.fact_id);c.field("task_id",f.task_id);c.field("contract_id",f.contract_id);c.field("acceptance_fact_id",f.acceptance_fact_id);c.field("action_id",f.action_id);c.field("action_kind",f.action_kind);c.field("provider_actor_id",f.provider_actor_id);c.field("place_id",f.place_id);c.field("reason",f.reason);c.field("started_tick",f.started_tick);c.field("stopped_tick",f.stopped_tick);c.field("completed_work_ticks",f.completed_work_ticks);return c.optional("cause_fact_id",f.cause_fact_id,[](auto& row,auto& value){return row.field("value",value);});}
// 表键保持原插入顺序，重复task键和未知模块版本均直接拒绝。
template<class A>Status state(A& c,EnvoyTaskState& s){c.field("version",s.version);if(s.version!=1&&s.version!=2)return c.reject("UnsupportedEnvoyTaskVersion");if(s.version==1)for(const auto& row:s.envoy_contracts.rows())if(row.second.completion&&row.second.completion->diplomatic_report_fact_id)return c.reject("EnvoyReportReferenceRequiresV2");return dynamic_codec::table(c,"envoy_contracts",s.envoy_contracts,[version=s.version](auto& row,auto& value){row.object("acceptance",[&](auto& nested){return acceptance(nested,value.acceptance);});row.optional("action",value.action,[](auto& nested,auto& value){return action(nested,value);});row.optional("delivery",value.delivery,[](auto& nested,auto& value){return delivery(nested,value);});row.optional("completion",value.completion,[version](auto& nested,auto& value){return completion(nested,value,version);});row.field("pending_code",value.pending_code);return row.sequence("interruptions",value.interruptions,[](auto& nested,auto& record){return interruption(nested,record);});});}
// JSON递归拒重复key；不能让第一个同名字段吞掉后续伪来源或未知版本。
Status unique_keys(const BoundaryValue& value,std::size_t depth=0){if(depth>CodecVisitor::max_depth)return common_failure("EnvoyPayloadTooDeep");if(const auto* fields=value.as<BoundaryValue::Dict>()){std::set<StableId> seen;for(const auto& field:*fields){if(!seen.insert(field.first).second)return common_failure("DuplicateEnvoyField",field.first);auto valid=unique_keys(field.second,depth+1);if(!valid.ok)return valid;}}if(const auto* rows=value.as<BoundaryValue::Array>()){if(rows->size()>CodecVisitor::max_items)return common_failure("EnvoyPayloadTooLarge");for(const auto& row:*rows){auto valid=unique_keys(row,depth+1);if(!valid.ok)return valid;}}return common_success();}
}
// 二进制与JSON走同一typed字段遍历，不改旧Common或Task visitor。
Status visit_envoy_task_checkpoint(CodecVisitor& c,EnvoyTaskState& s){return state(c,s);}
// 导出仅编码已有数据，权源与Task/Common的一致性由后继完整领域validator负责。
// 只写适配沿原 state visitor，DynamicProjectionArchive 不改源字段或复制整表。
Status emit_envoy_task_state_fields(ProjectionWriter& writer,const EnvoyTaskState& value){
    DynamicProjectionArchive archive(writer);
    return state(archive,const_cast<EnvoyTaskState&>(value));
}
// 原 JSON 编码与无树形状出口调用同一字段表；失败保完整原错误。
BoundaryValue encode_envoy_task_state(const EnvoyTaskState& value){
    BoundaryValue result;BoundaryProjectionWriter writer(result);
    auto status=writer.object("",[&](auto& fields){return emit_envoy_task_state_fields(fields,value);});
    if(status.ok)status=writer.finish();
    if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}
    return result;
}
// 解码严格拒缺字段、重复字段/键、未知字段和错误枚举，不默默补成功责任。
Outcome<EnvoyTaskState> decode_envoy_task_state(const BoundaryValue& value){auto valid=unique_keys(value);if(!valid.ok)return {false,{},valid.error};EnvoyTaskState result;dynamic_codec::JsonArchive codec(value);valid=state(codec,result);if(valid.ok)valid=codec.finish();return valid.ok?Outcome<EnvoyTaskState>{true,std::move(result),{}}:Outcome<EnvoyTaskState>{false,{},valid.error};}
}

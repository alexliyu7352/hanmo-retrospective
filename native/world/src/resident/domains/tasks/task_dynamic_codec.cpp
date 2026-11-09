// 动态任务的新section与旧任务section分离，旧档不会被静默补造合同或预留。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "task_dynamic_codec.hpp"
#include "task_dynamic_projection.hpp"
#include "task_projection.hpp"
namespace hanmo::resident {
Status visit_dynamic_task_checkpoint(CodecVisitor& codec,TaskState& state){return dynamic_codec::tasks(codec,state);}
// 原 dynamic_tasks 字段表逐项借读；写 JSON 与 shape 不复制整个 TaskState。
Status emit_dynamic_task_state_fields(ProjectionWriter& writer,const TaskState& state){
    DynamicProjectionArchive archive(writer);
    return dynamic_codec::tasks(archive,const_cast<TaskState&>(state));
}
BoundaryValue encode_dynamic_task_state(const TaskState& state){
    BoundaryValue value;BoundaryProjectionWriter writer(value);
    auto status=writer.object("",[&](auto& row){return emit_dynamic_task_state_fields(row,state);});
    if(status.ok)status=writer.finish();
    if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}
    return value;
}
Outcome<TaskState> decode_dynamic_task_state(const BoundaryValue& value,TaskState state){const auto source_deliveries=state.deliveries;const auto source_transfers=state.transfers;dynamic_codec::JsonArchive codec(value);auto status=dynamic_codec::tasks(codec,state);if(status.ok)status=codec.finish();if(status.ok&&!source_deliveries.empty()){if(source_deliveries.size()!=state.deliveries.size())status=common_failure("DynamicFactPayloadMismatch");for(const auto& row:source_deliveries.rows()){auto* loaded=state.deliveries.find(row.first);if(!loaded){status=common_failure("DynamicFactPayloadMismatch",row.first);break;}auto original=row.second;auto current=*loaded;dynamic_codec::JsonArchive a,b;dynamic_codec::delivery(a,original);dynamic_codec::delivery(b,current);if(!compare_boundary(a.value(),b.value()).exact()){status=common_failure("DynamicFactPayloadMismatch",row.first);break;}}}if(status.ok&&!source_transfers.empty()){if(source_transfers.size()!=state.transfers.size())status=common_failure("DynamicFactPayloadMismatch");for(const auto& row:source_transfers.rows()){auto* loaded=state.transfers.find(row.first);if(!loaded){status=common_failure("DynamicFactPayloadMismatch",row.first);break;}auto original=row.second;auto current=*loaded;dynamic_codec::JsonArchive a,b;dynamic_codec::transfer(a,original);dynamic_codec::transfer(b,current);if(!compare_boundary(a.value(),b.value()).exact()){status=common_failure("DynamicFactPayloadMismatch",row.first);break;}}}return status.ok?Outcome<TaskState>{true,std::move(state),{}}:Outcome<TaskState>{false,{},status.error};}
// 全局Fact导出只使用本域唯一收据；中央按真实Fact头合并，不另存一份货物状态。
namespace {
// 与原 cargo_disposition 同一字段 visitor；全局边界的 kind 先写，原行不增第二载荷。
Status emit_task_cargo_disposition_fields(ProjectionWriter& writer,const TaskCargoDispositionFact& fact){
    writer.text("kind","task_cargo_disposition");
    DynamicProjectionArchive archive(writer);
    return dynamic_codec::cargo_disposition(archive,const_cast<TaskCargoDispositionFact&>(fact));
}
}
Outcome<bool> emit_task_cargo_fact_payload_fields(ProjectionWriter& writer,const TaskState& state,const FactRecord& header){
    const auto* fact=state.cargo_dispositions.find(header.id);
    if(header.kind!="task_cargo_disposition"){
        if(fact)return {false,{},common_failure("TaskCargoFactKindMismatch",header.id,ErrorKind::internal).error};
        return {true,false,{}};
    }
    if(header.domain!="tasks"||!fact||fact->id!=header.id)
        return {false,{},common_failure("TaskCargoFactPayloadMismatch",header.id,ErrorKind::internal).error};
    auto status=emit_task_cargo_disposition_fields(writer,*fact);
    return status.ok?Outcome<bool>{true,true,{}}:Outcome<bool>{false,{},status.error};
}
Status emit_task_cargo_facts_fields(ProjectionWriter& writer,const TaskState& state){
    for(const auto& row:state.cargo_dispositions.rows()){
        if(row.first!=row.second.id)return writer.reject("TaskCargoFactPayloadMismatch",row.first);
        writer.object(row.first,[&](auto& child){return emit_task_cargo_disposition_fields(child,row.second);});
        if(!writer.status().ok)return writer.status();
    }
    return writer.status();
}
BoundaryValue encode_task_cargo_facts(const TaskState& state){
    BoundaryValue value;BoundaryProjectionWriter writer(value);
    auto status=writer.object("",[&](auto& row){return emit_task_cargo_facts_fields(row,state);});
    if(status.ok)status=writer.finish();
    if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}
    return value;
}

} // 冷decode载荷只建动态部分；完整任务绑定由中央在已有task基础上恢复。

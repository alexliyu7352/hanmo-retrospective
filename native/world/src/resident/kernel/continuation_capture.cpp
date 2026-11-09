// 只包原run的真实投影，不另生成action来源、许可条款或把Plan当Action续行。
#include "continuation_capture.hpp"
#include "world_impl.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
namespace {
// Snapshot的七个具名字符串在fresh构造前收费；分数由原BigNat实际allocator负责。

}
CheckpointJobResult<CapturedContinuation> ResidentWorld::capture_continuation_for_checkpoint(CheckpointJobBudget& job)const{
    try{
        if(!impl_->world)return {false,{},job.capture_boundary_failure("NoWorld",{},ErrorKind::rule)};
        auto memory=checkpoint_memory_resource(job,CheckpointJobUse::encode);if(!memory.ok)return {false,{},std::move(memory.error)};ScopedCheckpointMemory scope(memory.value);
        CapturedContinuation result;result.state_=checkpoint_make_unique<CapturedContinuation::State>();auto identity=job.reserve(CheckpointJobUse::encode,0);if(!identity.ok)return {false,{},std::move(identity.error)};result.state_->identity=std::move(identity.value);

        if(!impl_->plan_run){
            if(impl_->run){const auto& run=*impl_->run;
                if(impl_->world->catalog->exact_time){
                    if(exact_complete_selected(*impl_->world->catalog->exact_time)){
                        if(run.exact_action_started_source){}
                    }else for(const auto& row:impl_->world->growth.fixed_service_boundaries.rows())if(row.second.action_ref==run.request.action_id&&row.second.kind==LifeFixedBoundaryKind::started){
                        // 原rest producer的domain为SSO；kind使用原固定literal，含终止字节先收费。
                        (void)0;break;
                    }
                }
            }else (void)0;
        }
        (result.state_->strings).grow(0);
        auto captured=continuation_snapshot();if(!captured.ok)return {false,{},job.capture_failure(std::move(captured.error))};
        result.state_->value=std::move(captured.value);
        // 真实原reader判断没有可恢复Action时，未产生的字符串不继续占账。
        if(!result.state_->value)result.state_->strings={};
        return {true,std::move(result),{}};
    }catch(checkpoint_job_detail::FailureException& failure){return {false,{},std::move(failure.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
}

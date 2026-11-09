// 只核同捕获的实际执行与人物末值；业务原因、有效phase和永久全史由原领域另外提供。
#include "actor_condition_execution_coverage.hpp"
#include "common_store.hpp"
#include <limits>
namespace hanmo::resident {
struct ActorConditionCoverageBoundary::State {
    const CommonStore* original=nullptr;
    std::shared_ptr<const ActorConditionCaptureIdentity> capture_identity;
    WorldIdentity identity;
    Integer next_instance=0;
    OrderedTable<ActorConditionExecutionValues> actors;
    bool consumed=false;
};
namespace {
// 未写字段同样核前后不变；同值实写靠presence保留，不按净差猜一次操作。
bool same(const ActorConditionExecutionValues& a,const ActorConditionExecutionValues& b){return a.hp==b.hp&&a.fatigue==b.fatigue&&a.injury==b.injury&&a.alive==b.alive&&a.available==b.available;}
// 原人物表主键与记录身份一致；不从当前表创建原业务初态证明。
Outcome<OrderedTable<ActorConditionExecutionValues>> project(const CommonState& state){
    OrderedTable<ActorConditionExecutionValues> out;
    for(const auto& row:state.actors.rows())if(row.first.empty()||row.second.id!=row.first||!out.insert(row.first,{row.second.hp,row.second.fatigue,row.second.injury,row.second.alive,row.second.available}))return {false,{},common_failure("ActorConditionCoverageActorMismatch",row.first).error};
    return {true,std::move(out),{}};
}
}
Outcome<ActorConditionCoverageBoundary> begin_actor_condition_coverage(CommonStore& common){
    auto before=project(common.state());if(!before.ok)return {false,{},before.error};
    auto begun=common.begin_actor_condition_capture();if(!begun.ok)return {false,{},begun.error};
    auto state=std::make_shared<ActorConditionCoverageBoundary::State>();state->original=&common;state->capture_identity=common.actor_condition_capture_identity();state->identity=common.state().identity;state->next_instance=common.state().next_instance_id;state->actors=std::move(before.value);
    ActorConditionCoverageBoundary out;out.state_=std::move(state);return {true,std::move(out),{}};
}
Outcome<std::vector<ActorConditionExecutionDraft>> finish_actor_condition_coverage(CommonStore& common,ActorConditionCoverageBoundary& boundary){
    const auto fail=[](const StableId& code,const StableId& actor=StableId{})->Outcome<std::vector<ActorConditionExecutionDraft>>{return {false,{},common_failure(code,actor).error};};
    auto state=boundary.state_;if(!state||state->consumed||state->original!=&common||!state->capture_identity||state->capture_identity!=common.actor_condition_capture_identity())return fail("ActorConditionCoverageBoundaryRequired");
    // 原保存点未闭时只拒绝本次收尾，不能拿走草稿或消耗可重试边界。
    auto captured=common.finish_actor_condition_capture();if(!captured.ok)return captured;state->consumed=true;
    const auto& final=common.state();if(state->identity.world_id!=final.identity.world_id||state->identity.execution_epoch!=final.identity.execution_epoch||state->identity.tick>final.identity.tick||state->identity.revision>final.identity.revision||state->next_instance>final.next_instance_id)return fail("ActorConditionCoverageIdentityMismatch");
    auto expected=state->actors;Integer prior_id=state->next_instance,prior_tick=state->identity.tick,prior_revision=state->identity.revision,sequence=0;
    for(const auto& row:captured.value){
        if(sequence==std::numeric_limits<Integer>::max()||row.sequence!=++sequence||row.actor_id.empty()||row.identity.world_id!=state->identity.world_id||row.identity.execution_epoch!=state->identity.execution_epoch||row.identity.tick<prior_tick||row.identity.tick>final.identity.tick||row.identity.revision<prior_revision||row.identity.revision>final.identity.revision||row.next_instance_before<prior_id||row.next_instance_after!=row.next_instance_before||row.next_instance_after>final.next_instance_id)return fail("ActorConditionCoverageDraftMismatch",row.actor_id);
        const auto* old=expected.find(row.actor_id);
        if(row.kind==ActorConditionExecutionDraft::Kind::mutation){
            if(!old||!row.before||!same(*old,*row.before)||(!row.hp_written&&!row.fatigue_written&&!row.injury_written&&!row.alive_written&&!row.available_written)||(!row.hp_written&&row.before->hp!=row.after.hp)||(!row.fatigue_written&&row.before->fatigue!=row.after.fatigue)||(!row.injury_written&&row.before->injury!=row.after.injury)||(!row.alive_written&&row.before->alive!=row.after.alive)||(!row.available_written&&row.before->available!=row.after.available))return fail("ActorConditionCoverageGap",row.actor_id);
        }else if(row.kind==ActorConditionExecutionDraft::Kind::creation){
            if(old||row.before||!row.hp_written||!row.fatigue_written||!row.injury_written||!row.alive_written||!row.available_written)return fail("ActorConditionCoverageCreationMismatch",row.actor_id);
        }else return fail("ActorConditionCoverageKindMismatch",row.actor_id);
        expected.set(row.actor_id,row.after);prior_id=row.next_instance_after;prior_tick=row.identity.tick;prior_revision=row.identity.revision;
    }
    auto actual=project(final);if(!actual.ok)return {false,{},actual.error};if(expected.order()!=actual.value.order())return fail("ActorConditionCoverageTableMismatch");
    for(const auto& row:expected.rows()){const auto* value=actual.value.find(row.first);if(!value||!same(row.second,*value))return fail("ActorConditionCoverageGap",row.first);}
    return captured;
}
}

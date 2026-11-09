// 只消费同Common真实capture：所有持久业务因果、直接写者保护与跨提交保留仍须另闭。
#include "resource_execution_coverage.hpp"
#include "common_store.hpp"
#include <set>
namespace hanmo::resident {
struct ResourceExecutionCoverageBoundary::State {
    const CommonStore* original=nullptr;
    std::shared_ptr<const ResourceExecutionCaptureIdentity> capture_identity;
    WorldIdentity identity;
    Integer next_instance=0;
    OrderedTable<WalletView> wallets;
    OrderedTable<InventoryView> inventories;
    bool consumed=false;
};
namespace {
// 拥有值含兼容字段和原顺序；等额或同Owner主键不替代真实原值。
bool owner_same(const Owner& a,const Owner& b){return owner_equal(a,b)&&a.extra==b.extra&&a.field_order==b.field_order;}
bool wallet_same(const WalletView& a,const WalletView& b){
    if(a.id!=b.id||!owner_same(a.owner,b.owner)||a.balance!=b.balance||a.field_order!=b.field_order||a.reserved.size()!=b.reserved.size())return false;
    for(std::size_t i=0;i<a.reserved.size();++i)if(a.reserved[i].action_id!=b.reserved[i].action_id||a.reserved[i].amount!=b.reserved[i].amount)return false;
    return true;
}
bool lot_same(const LotView& a,const LotView& b){return a.id==b.id&&a.item_id==b.item_id&&a.quantity==b.quantity&&owner_same(a.owner,b.owner)&&a.reserved_by==b.reserved_by&&a.field_order==b.field_order&&a.extra==b.extra;}
bool inventory_same(const InventoryView& a,const InventoryView& b){
    if(a.id!=b.id||!owner_same(a.owner,b.owner)||a.capacity!=b.capacity||a.revision!=b.revision||a.field_order!=b.field_order||a.lots.size()!=b.lots.size())return false;
    for(std::size_t i=0;i<a.lots.size();++i)if(!lot_same(a.lots[i],b.lots[i]))return false;
    return true;
}
// 每个实际帧必须接上已观察到的上一值；保原创建顺序，不自行重放或发明recipe。
template<class T,class Frame,class Equal,class Key>
Status fold_frames(OrderedTable<T>& expected,const std::vector<Frame>& frames,Equal equal,Key key){
    std::set<StableId> touched;
    for(const auto& frame:frames){
        const auto& id=key(frame);const auto* old=expected.find(id);
        if(id.empty()||!touched.insert(id).second||frame.after.id!=id||bool(old)!=bool(frame.before)||(old&&!equal(*old,*frame.before)))
            return common_failure("ResourceExecutionCoverageGap",id);
        expected.set(id,frame.after);
    }
    return common_success();
}
// 全表比较包括未触资源、批次顺序与兼容字段，不能只检查净额或revision。
template<class T,class Equal>
Status match_table(const OrderedTable<T>& expected,const OrderedTable<T>& actual,Equal equal){
    if(expected.order()!=actual.order())return common_failure("ResourceExecutionCoverageTableMismatch");
    for(const auto& row:expected.rows()){const auto* value=actual.find(row.first);if(!value||!equal(row.second,*value))return common_failure("ResourceExecutionCoverageGap",row.first);}
    return common_success();
}
}
Outcome<ResourceExecutionCoverageBoundary> begin_resource_execution_coverage(CommonStore& common){
    auto begun=common.begin_resource_execution_capture();if(!begun.ok)return {false,{},begun.error};
    ResourceExecutionCoverageBoundary boundary;auto state=std::make_shared<ResourceExecutionCoverageBoundary::State>();
    state->original=&common;state->capture_identity=common.resource_execution_capture_identity();state->identity=common.state().identity;state->next_instance=common.state().next_instance_id;
    state->wallets=common.state().wallets;state->inventories=common.state().inventories;boundary.state_=std::move(state);
    return {true,std::move(boundary),{}};
}
Outcome<std::vector<ResourceExecutionDraft>> finish_resource_execution_coverage(CommonStore& common,ResourceExecutionCoverageBoundary& boundary){
    auto fail=[](const StableId& code,const StableId& id=StableId{})->Outcome<std::vector<ResourceExecutionDraft>>{return {false,{},common_failure(code,id).error};};
    auto state=boundary.state_;
    if(!state||state->consumed||state->original!=&common||!state->capture_identity||state->capture_identity!=common.resource_execution_capture_identity())return fail("ResourceExecutionCoverageBoundaryRequired");
    // 原capture仍有保存点/正在原语执行时不可消费，保持原Common的拒绝与可重试边界。
    auto captured=common.finish_resource_execution_capture();if(!captured.ok)return captured;state->consumed=true;
    const auto& final=common.state();
    if(state->identity.world_id!=final.identity.world_id||state->identity.execution_epoch!=final.identity.execution_epoch||state->identity.tick>final.identity.tick||state->identity.revision>final.identity.revision||state->next_instance>final.next_instance_id)
        return fail("ResourceExecutionCoverageIdentityMismatch");
    auto wallets=state->wallets;auto inventories=state->inventories;Integer prior_id=state->next_instance;auto prior_tick=state->identity.tick;auto prior_revision=state->identity.revision;
    for(const auto& draft:captured.value){
        if(draft.identity.world_id!=state->identity.world_id||draft.identity.execution_epoch!=state->identity.execution_epoch||draft.identity.tick<prior_tick||draft.identity.tick>final.identity.tick||draft.identity.revision<prior_revision||draft.identity.revision>final.identity.revision||draft.next_instance_before<prior_id||draft.next_instance_after<draft.next_instance_before||draft.next_instance_after>final.next_instance_id||draft.operations.size()!=draft.recipe.operations.size())return fail("ResourceExecutionCoverageDraftMismatch");
        Integer operation_id=draft.next_instance_before;
        for(std::size_t i=0;i<draft.operations.size();++i){
            const auto& op=draft.operations[i];
            if(op.operation_index!=i||op.next_instance_before!=operation_id||op.next_instance_after<op.next_instance_before||op.next_instance_after>draft.next_instance_after)return fail("ResourceExecutionCoverageOperationMismatch");
            auto valid=fold_frames(wallets,op.wallets,wallet_same,[](const auto& v)->const StableId&{return v.wallet_id;});if(!valid.ok)return {false,{},valid.error};
            valid=fold_frames(inventories,op.inventories,inventory_same,[](const auto& v)->const StableId&{return v.inventory_id;});if(!valid.ok)return {false,{},valid.error};operation_id=op.next_instance_after;
        }
        if(operation_id!=draft.next_instance_after)return fail("ResourceExecutionCoverageOperationMismatch");prior_id=draft.next_instance_after;prior_tick=draft.identity.tick;prior_revision=draft.identity.revision;
    }
    auto valid=match_table(wallets,final.wallets,wallet_same);if(!valid.ok)return {false,{},valid.error};
    valid=match_table(inventories,final.inventories,inventory_same);if(!valid.ok)return {false,{},valid.error};
    return captured;
}
}

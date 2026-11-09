// 原CancelCraft：停止未来服务、释放原锁、整item一次50%回收并实际结束同Job。
#pragma once
#include "craft_job.hpp"
namespace hanmo::resident {
enum class CraftCancellationAdoptionCheck {new_adoption,existing_source};
class CraftCancellationOriginalRead {
public:
 virtual ~CraftCancellationOriginalRead()=default;
 // 只反读原接受政策/本人或合法原商品授权；不得把专业服务借底层取消。
 virtual Status validate_craft_cancellation_scope(const CraftJob&,const CraftCancellationSource&,CraftCancellationAdoptionCheck)const=0;
 // 固定已接受取消/产物权属条款，不从今天余料或当前Owner推回收属性。
 virtual Outcome<CraftRecoveryTerms> read_craft_recovery_terms(const CraftJob&,const StableId& definition_id)const=0;
 virtual Status validate_craft_recovery_destination(const CraftJob&,const CraftRecoveryTerms&,const WorldMoment&,const SourceRef* actual_result)const=0;
 // 实际设施有限保留与费用/旧债处置由原owner核；不得回调全Craft时间reader。
 virtual Status validate_craft_cancellation_disposition(const CraftJob&,const CraftCancellationSource&)const=0;
 virtual Status validate_craft_cancellation_settlement(const CraftJob&,const CraftCancellationResult&)const=0;
};
class CraftCancellationEffectsWrite {
public:
 virtual ~CraftCancellationEffectsWrite()=default;
 virtual bool bound_to(const CommonStore&,const CraftJobsState&)const noexcept=0;
 virtual Outcome<std::vector<MomentOwnerSource>> adopt_craft_cancellation_in_candidate(const CraftJob&,const CraftCancellationSource&)=0;
 virtual Outcome<std::vector<MomentOwnerSource>> settle_craft_cancellation_in_candidate(const CraftJob&,const CraftCancellationResult&)=0;
};
struct CancelCraftRequest {
 StableId job_id,actor_id;Integer expected_gate_ordinal=0;
 std::optional<StableId> expected_last_closed_gate_ref;
};
struct CancelCraftResult {StableId cancellation_ref;std::optional<StableId> actual_settlement_ref;};
// 原源局部反读不要求已发生其后物化/设施后果，允许同候选严格先源后果。
Outcome<const CraftCancellationSource*> read_craft_cancellation_source(const CraftJobReadContext&,const StableId&);
Outcome<const CraftCancellationResult*> read_craft_cancelled_for_output(const CraftJobReadContext&,const StableId&);
Status validate_craft_cancellation_materialization(const CraftJobReadContext&,const CraftCancellationResult&);
Status validate_craft_cancellations(const CraftJobReadContext&);
Outcome<ItemOutputSourceView> read_cancelled_craft_item_output(const CraftJobReadContext&,const ItemOutputKey&);
Outcome<bool> read_craft_cancellation_ready(const CraftJobReadContext&,const StableId& job_id);
Outcome<CancelCraftResult> cancel_craft_in_candidate(CommonStore&,CraftJobsState&,ActionElapsedStore&,ItemInstancesState&,LotPropertiesState&,const CraftJobWriteContext&,const ItemOutputSourceRead&,const CancelCraftRequest&);
Outcome<std::optional<StableId>> settle_craft_cancellation_in_candidate(CommonStore&,CraftJobsState&,ActionElapsedStore&,ItemInstancesState&,LotPropertiesState&,const CraftJobWriteContext&,const ItemOutputSourceRead&,const StableId& job_id);
}

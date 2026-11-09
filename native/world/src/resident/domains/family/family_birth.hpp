// 生育安排、唯一出生和逐次照护各自记账；有限备粮不能伪装成儿童已被照料。
#pragma once
#include "birth_arrangement_source.hpp"
#include "birth_care_resources.hpp"
#include "family_lineage.hpp"
#include "birth_due_execution.hpp"
#include <set>
namespace hanmo::resident {
struct FamilyDeathState;class BirthPlacementRead;
// 只保存本人实际听到的成年双方条款，收到本身不预约粮食。
struct BirthProposal { StableId kind="family_birth_proposed";BirthArrangementSource source;FieldOrder field_order; };
// 接受必须重新取得同一条款的当前本人节点；拒绝/撤回没有第二人的伪造确认。
struct BirthReply { StableId kind="family_birth_replied",proposal_id,actor_id,decision;Integer tick=0,revision=0;std::optional<BirthArrangementSource> confirmation;Integer reserved_food=0,due_tick=0;FieldOrder field_order; };
// 原安排终止仅取消尚未兑现的计划或剩余有限资助，已出生的人物和已消费口粮始终保留。
struct BirthClosure { StableId kind="family_birth_closed",accepted_id,actor_id,reason,source_fact_id;Integer tick=0,revision=0,released_food=0;FieldOrder field_order; };
// 出生事实永久固定原父母、原抽样和新人的身份，改名、查询或读档不得再次生成。
struct FamilyBirthRecord { StableId kind="family_birth_recorded",accepted_id,actor_id,prototype_id,display_name,place_id;Integer tick=0,revision=0;StableId rng_stream="world",rng_backend;Integer rng_seed=0,rng_before=0,rng_after=0;std::vector<StableId> biological_parent_ids;OrderedTable<BirthInnateResult> innate;FieldOrder field_order;std::optional<MomentOwnerSource> location_result_ref;StableId allocation_source_ref; };
// 原Action开工源永久保留；儿童是受照护对象，不成为同意劳务或支付的行动参与者。
struct ChildCareStart { StableId kind="family_child_care_started",accepted_id,child_actor_id,caregiver_actor_id,action_id,place_id;Integer tick=0,revision=0,duration=0,period_index=0;FieldOrder field_order; };
// 必须有实际独立照护Action开始与完成；原粮来源、每期数量和本次照护对象精确绑定。
struct ChildCareReceipt { StableId kind="family_child_care",accepted_id,child_actor_id,caregiver_actor_id,action_id,place_id,outcome="completed",code,interruption_actor_id,interruption_fact_id;Integer started_tick=0,tick=0,revision=0,period_index=0,food_consumed=0;FieldOrder field_order; };
// 成年事实只确认原年龄；availability_enabled明确记录是否取得当时可自由行动的独立来源。
struct ChildMaturationRecord { StableId kind="family_child_matured",actor_id,birth_fact_id;Integer tick=0,revision=0;bool availability_enabled=false;FieldOrder field_order; };
struct FamilyBirthState { OrderedTable<BirthProposal> proposals;OrderedTable<BirthReply> replies;OrderedTable<BirthClosure> closures;OrderedTable<FamilyBirthRecord> births;OrderedTable<ChildCareReceipt> care;OrderedTable<ChildCareStart> care_starts;OrderedTable<ChildMaturationRecord> maturations;FieldOrder field_order;OrderedTable<BirthDueExecution> birth_due_executions; };
// 回调只读同一候选的真实历史边界。初始亲缘和军事处境须已验证，未知来源直接拒绝。
struct BirthArrangementAccess {
    const FamilyLineageCatalog* lineage=nullptr;
    std::function<Outcome<StableId>(Integer,const StableId&)> player_before;
    std::function<Status(const StableId&,Integer,const StableId&)> decision_maker_before;
    std::function<Status(const StableId&,Integer,const StableId&)> care_recipient_alive_before;
    std::function<Status(const StableId&,const StableId&,Integer,const StableId&)> eligible_pair_before;
    // 必须先过完整FamilyLife/军事死因核验；缺本账不得用当前alive=false伪造照护中止。
    const FamilyDeathState* life=nullptr;
    std::function<Status(const StableId&,Integer,const StableId&)> maturation_unoccupied_before;
    // 显式接手profile才借此纯前缀互斥；旧Birth访问不增义务，完整Relief史仍在Birth之后验证。
    bool care_relief_profile_enabled=false;
    std::function<Status(const StableId&,Integer,const StableId&)> original_care_not_replaced_before;
    const FamilyContinuationPeopleRead4* people=nullptr;
    const BirthDueBornSourceRead* born_sources=nullptr;
    const AuthoredActorFoundationState* foundation=nullptr;
    const ActorFoundationCatalog* foundation_catalog=nullptr;
    const BirthPlacementRead* placement=nullptr;
};
// 合同存在性只认本账的真实接受记录，Common同名头本身不产生预约权限。
bool family_birth_reservation_exists(const CommonStore&,const FamilyBirthState&,const ReservationRef&);
// 仅已核前段安排和预约的投影。出生/照护完整validator随后按原事实扩展，未支持实体严格拒绝。
struct BirthArrangementTimeline { std::set<StableId> replied_proposals,closed_acceptances,born_acceptances;OrderedTable<Integer> remaining_food,completed_care_periods;OrderedTable<StableId> pending_care_actions; };
Outcome<BirthArrangementTimeline> validate_birth_arrangements(const CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,const FamilyBirthState&,const BirthArrangementAccess&,const StableId& finishing_care_action={});
Outcome<StableId> receive_birth_proposal(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const BirthArrangementQuote&);
Outcome<StableId> reply_birth_proposal(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const StableId& proposal_id,const StableId& decision,const std::optional<BirthArrangementQuote>& confirmation);
// 当前仅未出生安排允许本人撤回；已有儿童的资助终止须走后继有照护责任的显式路径。
Outcome<StableId> withdraw_birth_arrangement(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const StableId& accepted_id);
Outcome<FamilyBirthState> decode_family_birth(const BoundaryValue&);
BoundaryValue encode_family_birth(const FamilyBirthState&);
Status visit_family_birth_checkpoint(CodecVisitor&,FamilyBirthState&);
Outcome<FamilyBirthState> decode_family_birth_complete(const BoundaryValue&,Integer selected_version);
BoundaryValue encode_family_birth_complete(const FamilyBirthState&,Integer selected_version);
Status visit_family_birth_checkpoint_complete(CodecVisitor&,FamilyBirthState&,Integer selected_version);
// 整个World候选或外层Common保存点专用；调用方必须备份全部typed写集，任一失败整体丢弃/回滚。
// 不自行开启或提交保存点；不得把失败的候选继续用于后续命令。
// 本入口写Common和FamilyBirth，其他领域只读。
Outcome<StableId> receive_birth_proposal_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const BirthArrangementQuote&);
Outcome<StableId> reply_birth_proposal_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const StableId& proposal_id,const StableId& decision,const std::optional<BirthArrangementQuote>& confirmation);
Outcome<StableId> withdraw_birth_arrangement_in_candidate(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const StableId& accepted_id);
}

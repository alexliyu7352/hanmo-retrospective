// 完整Time2只复用原WorldBundle候选/发布口；本栈借用由组合层从同一候选实际状态构造。
#pragma once
#include "exact_time_profile.hpp"
#include "resident/facility_claims/facility_claim_runtime.hpp"
#include "resident/domains/director/director_exact_runtime.hpp"
#include "resident/domains/actor_foundation/character_condition_world.hpp"
#include "resident/domains/local_economy/economic_exact_phase.hpp"
namespace hanmo::resident {
struct NativeCompleteWorldCoreSources;
class GrowthCapabilityRevisionWrite;class WorkCompletedResultConsumer;
class CraftExactReadLease;class CraftExactWriteLease;
class RepairExactReadLease;class RepairExactWriteLease;
class CraftFacilityReadLease;class CraftFacilityWriteLease;
class HouseholdServiceReadLease;class HouseholdServiceWriteLease;
class NativeHouseholdFoundationLease;
class MotionExactReadLease;class MotionExactWriteLease;
class StateNeedReadLease;class StateNeedWriteLease;
class TaskWorkReadLease;class TaskWorkWriteLease;
class MilitaryExactReadLease;class MilitaryExactWriteLease;
class PoliticalExactReadLease;class PoliticalExactWriteLease;
class CareLifeConditionWrite;class GrowthStartedParticipationWrite;
class OriginalGrowthServiceRelationWrite;class OriginalTaskReportRelationWrite;class OriginalSocialCalendarRelationWrite;
class NativeOriginalSocialWorldRead;class NativeContinuationOriginalSourceRead;
class FamilyContinuationPeopleRead4;struct ProfileControlEligibilitySources;class LifeAgeExactReadLease;
struct PrivateShopOperationSourceContext;struct BornCharacterSourceContext;
class NativeInteractionWorldRead;class MilitaryFormationSourceRead;class MilitaryReturnSourceRead;class NativeMilitaryTrainingPeriodRead;
class NativeTransportInventoryEntryRead;class TransportManifestHistorySourceRead;
struct PreparedDomainOp;struct ReceiptDetails;struct CommitResult;class GrowthPendingStartedProof;
struct WorldBundle;struct AdvanceDetails;struct HouseholdPurchaseContext;struct HouseholdRetailReadContext;struct PopulationMilitaryReadContext;
// 所有引用绑定同一候选；不能持久、跨clone借用或由外部command提交。runtime来自compiled compose。
struct CompleteExactWorldReadBindings {
    const CompleteExactTimeRuntime& runtime;
    const ActionTimeOriginalSourceRead& action_sources;
    const FacilityClaimsState& facility_claims;
    const FacilityClaimSourceRead& facility_sources;
    const FacilityHolderSourceRead& facility_holders;
    const std::vector<FacilityHolderRegistration>& facility_holder_handlers;
    const MovementExactState& movement;
    DirectorExactWorldFactory director;
    const CharacterConditionWorldRead& character_conditions;
    const HouseholdPurchaseContext* household_purchases=nullptr;
    const HouseholdRetailReadContext* household_retail=nullptr;
    const PopulationMilitaryReadContext* precise_population=nullptr;
    // 仅实际 S5 owning 工厂；未选择为空，选中缺一口即技术拒绝。
    EconomicExactWorldFactory economics;
    // 借同一实际 Family owner；赴约2等选中时必须非空，不能现场造空生命源。
    const FamilyContinuationPeopleRead4* family_people4=nullptr;
    const ProfileControlEligibilitySources* family_control_sources=nullptr;
    // Decoded immutable manifests borrow original S1/S2/P02 readers from this
    // same lease. Construction must not validate still-unhydrated totals.
    const NativeTransportInventoryEntryRead* transport_inventory_entries=nullptr;
    const TransportManifestHistorySourceRead* transport_manifest_history=nullptr;
    // Same original age owner that supplies body Life4 and Family people.
    const LifeAgeExactReadLease* family_life_age=nullptr;
    // Actual S3 subgroup, constructed after the same Family/body owners and
    // before P10. Its constructor never captures a pending zero-step result.
    const NativeInteractionWorldRead* interaction_sources=nullptr;
    // The same true S2 formation reader also supplies original Army roster
    // revisions. It is borrowed, never recovered from today's Army state.
    const MilitaryFormationSourceRead* military_formations=nullptr;
    // Return history and captured placement borrow the same real S2 return
    // owner. A completed action is not replayed through the legacy effect writer.
    const MilitaryReturnSourceRead* military_returns=nullptr;
    // Original supply occurrence and Army roster fold use the same economic
    // Opening adapter as body shortage consequences; Supply.tick is only a label.
    const NativeMilitaryTrainingPeriodRead* military_supply_periods=nullptr;
    // Same lease owns the original Social source reader after the body owner.
    const NativeOriginalSocialWorldRead* original_social=nullptr;
    // Same original Family graph, including its real Control2/3 root and cuts.
    const NativeContinuationOriginalSourceRead* continuation_sources=nullptr;
    // Original Shop2/P29 context and every borrowed authority live in this owner.
    const PrivateShopOperationSourceContext* private_shop_operations=nullptr;
    // Original creation and full publication legs borrow this one Family owner.
    const BornCharacterSourceContext* family_original_birth=nullptr;
    // 同一不移动 Complete owner 的实际核心借图，不可跨候选复用。
    const NativeCompleteWorldCoreSources* original_core=nullptr;
    // 原 Shop2 命令/阶段只借同 World、同 M 家户根；寿命不得超过本 owner。
    const NativeHouseholdFoundationLease* household_foundation=nullptr;
};
// 拥有本次全部adapter，reads()内引用只在本lease寿命内有效；不得跨clone/cache保存。
class CompleteExactWorldBindingOwner {
public:
    virtual ~CompleteExactWorldBindingOwner()=default;
    virtual const CompleteExactWorldReadBindings& reads()const=0;
    // Direct domain acquisition, not a call back through reads().economics or
    // any compiled public factory. A derived owner supplies its genuine domain
    // assembly; there is no empty/default-success implementation here.
    // Each returned inner lease owns its actual-M snapshot contexts. The native
    // outer factory retains this complete owner until that inner lease is gone.
    virtual Outcome<std::shared_ptr<const EconomicExactReadLease>> read_economic(const WorldMoment&)const=0;
    virtual Outcome<std::shared_ptr<EconomicExactWriteLease>> write_economic(const WorldMoment&,const DynamicStepInput*)=0;
    virtual Outcome<std::shared_ptr<const CraftExactReadLease>> read_craft(const WorldMoment&)const=0;
    virtual Outcome<std::shared_ptr<CraftExactWriteLease>> write_craft(const WorldMoment&,ExactPhaseWriteContext&)=0;
    virtual Outcome<std::shared_ptr<const RepairExactReadLease>> read_repair(const WorldMoment&)const=0;
    virtual Outcome<std::shared_ptr<RepairExactWriteLease>> write_repair(const WorldMoment&,ExactPhaseWriteContext&)=0;
    // Facility callbacks retain their genuine specialist owner independently.
    // Household does not require a Production/Economic calendar selection.
    virtual Outcome<std::shared_ptr<const CraftFacilityReadLease>> read_craft_facility(const WorldMoment&)const=0;
    virtual Outcome<std::shared_ptr<CraftFacilityWriteLease>> write_craft_facility(const WorldMoment&,ExactPhaseWriteContext&)=0;
    virtual Outcome<std::shared_ptr<const HouseholdServiceReadLease>> read_household_service(const WorldMoment&)const=0;
    virtual Outcome<std::shared_ptr<HouseholdServiceWriteLease>> write_household_service(const WorldMoment&)=0;
    // These are the original remaining typed domain leases, not substitute
    // business providers. Motion must use the non-reacquiring in-owner route.
    virtual Outcome<std::shared_ptr<const MotionExactReadLease>> read_motion(const WorldMoment&)const=0;
    virtual Outcome<std::shared_ptr<MotionExactWriteLease>> write_motion(const WorldMoment&)=0;
    virtual Outcome<std::shared_ptr<const StateNeedReadLease>> read_state_need(const WorldMoment&)const=0;
    virtual Outcome<std::shared_ptr<StateNeedWriteLease>> write_state_need(const WorldMoment&)=0;
    virtual Outcome<std::shared_ptr<const PoliticalExactReadLease>> read_political(const WorldMoment&)const=0;
    virtual Outcome<std::shared_ptr<PoliticalExactWriteLease>> write_political(const WorldMoment&)=0;
    virtual Outcome<std::shared_ptr<const DirectorExactReadLease>> read_director(const WorldMoment&)const=0;
    virtual Outcome<std::shared_ptr<DirectorExactWriteLease>> write_director(const WorldMoment&)=0;
    // The original Task/Work source graph is acquired without re-entering the
    // complete World getter. Its inner lease owns the actual-M context values.
    virtual Outcome<std::shared_ptr<const TaskWorkReadLease>> read_task_work(const WorldMoment&)const=0;
    virtual Outcome<std::shared_ptr<TaskWorkWriteLease>> write_task_work(const WorldMoment&)=0;
    // The one Military phase borrows its actual original contexts from this
    // owner; the inner acquisition never calls the public factory recursively.
    virtual Outcome<std::shared_ptr<const MilitaryExactReadLease>> read_military(const WorldMoment&)const=0;
    virtual Outcome<std::shared_ptr<MilitaryExactWriteLease>> write_military(const WorldMoment&,ExactPhaseWriteContext&)=0;
    // Legacy and const leases expose no writer. Explicit phase4 must obtain
    // the actual same-candidate concrete ports from the mutable owning factory.
    virtual GrowthCapabilityRevisionWrite* growth_capability_writer(){return nullptr;}
    virtual WorkCompletedResultConsumer* work_completed_growth_consumer(){return nullptr;}
    // Borrow the original Care owner from this mutable candidate. Life writers
    // consume each actual token here, before the next actor, never in a late sweep.
    virtual CareLifeConditionWrite* care_life_writer(){return nullptr;}
    // 同候选原照护参与采用者；const 报价不借写端口。实际开工在分配 Action
    // 前核必需性，真实 seal/FSL 尾再借同候选端口消费，不缓存跨世界的许可。
    virtual GrowthStartedParticipationWrite* growth_start_participation_writer(){return nullptr;}
    // The actual Growth phase borrows this from the same mutable lease.
    // Social2 requires its original completion source; const and unselected
    // leases expose no writer, and must not synthesize a relation effect.
    virtual OriginalGrowthServiceRelationWrite* original_growth_service_relation_writer(){return nullptr;}
    virtual OriginalTaskReportRelationWrite* original_task_report_relation_writer(){return nullptr;}
    virtual OriginalSocialCalendarRelationWrite* original_social_calendar_relation_writer(){return nullptr;}
};
// 只投影已实际开启的当前期；Ready没有未来period ID。业务仍反读自己的冻结集合/真实服务源。
Outcome<std::optional<ExactPeriodContext>> read_complete_exact_open_period(const WorldBundle&,ExactArithmeticBudget={});
Status validate_complete_exact_time_world(const WorldBundle&,const CompleteExactWorldReadBindings&,ExactArithmeticBudget={});
Outcome<WorldMoment> prepare_complete_exact_advance_target(const WorldBundle&,const CompleteExactWorldReadBindings&,const std::optional<WorldMoment>& requested,ExactArithmeticBudget={});
// Consume only actual S1 source heads already written in this live candidate
// capture. Motion calls this before its subsequent exertion Condition write.
// The caller retains the real phase scope and the single outer write capture.
// 原 LifeAge/伤病 writer 的实际 token 已即时推进能力链；此口只消费本次真实身体输入。
Status consume_complete_growth_condition_inputs_in_candidate(WorldBundle&,ExactPhaseWriteContext&,ExactArithmeticBudget={});
Status consume_complete_growth_spatial_inputs_in_candidate(WorldBundle&,ExactPhaseWriteContext&,ExactArithmeticBudget={},const GrowthPendingStartedProof* =nullptr);
// Called after a real original Interaction/Party source is appended, while the
// same outer candidate capture is live; never infer a change from current emptiness.
Status consume_complete_growth_quiet_inputs_in_candidate(WorldBundle&,ExactPhaseWriteContext&,ExactArithmeticBudget={},const GrowthPendingStartedProof* =nullptr);
// Original command / advance / commit-tail capture. Its state is private to
// this candidate and never survives the owning submission stack.
class CompleteExactCommandCapture final:public CheckpointAllocatedObject {
    struct State;std::unique_ptr<State> state_;
    explicit CompleteExactCommandCapture(std::unique_ptr<State>);
public:
    ~CompleteExactCommandCapture();
    static Outcome<std::unique_ptr<CompleteExactCommandCapture>> begin(WorldBundle&);
    bool bound_to(const WorldBundle&)const noexcept;
    Status settle();
    // 原采用前冻结输入，采用后监看人物/行动，原业务头追加后再保护其身份。
    Status prepare_pending_growth_start(const ActionCoreView&);
    Status observe_pending_growth_adoption(const StableId&);
    Status observe_pending_growth_business(const StableId&);
    // 真实提交尾联接旧根、ActionStarted和序号；未联接不得参与封存前消费。
    Status identify_pending_growth_start(const WorldBundle*,const StableId&,Integer,const ReceiptDetails&);
    // 只包装原真实封存结果绑定；验证并关闭唯一待绑定证明。
    Status bind_growth_command_result(const WorldBundle*,const StableId&,Integer,const CommitResult&);
    // 仅原私店开工/取消借同候选真 capture；取消 opaque token 留到实际 seal。
    Outcome<ReceiptDetails> apply_private_shop_command(const StableId&,const PreparedDomainOp&);
    // 真实命令结果封口后绑定原取消终局；不得猜未来 result 或重建家户租约。
    Status bind_private_shop_command_result(const SourceRef& actual_result);
    // The real Common seal already exists; this delegates the unique original
    // Interaction pump while the same candidate and write capture remain live.
    Status finalize_selected_interaction_response(const SourceRef& actual_result);
    // Bind only original death closures captured before this actual Common seal.
    // No dead dialogue pump, future result ID, or second result ledger.
    Status bind_interaction_session_death_result(const SourceRef& actual_result);
    // The real outer ActionStarted seal is retained before the original Growth
    // started-command reader verifies its nested accepted business terms.
    Status bind_interaction_action_confirmation_result(const SourceRef& actual_result);
    Status finish();
};
// Opening独立候选；回调均成功、原owner源反验及身体/位置capture齐备后才可由原Impl发布。
Status open_complete_exact_period_in_candidate(WorldBundle&,const CompleteExactWorldReadBindings&,ExactArithmeticBudget={});
// 仅处理一次原advance所允许的实际最早片；不会循环自动开下一期或消费额外run许可。
Outcome<AdvanceDetails> advance_complete_exact_world(WorldBundle&,const CompleteExactWorldReadBindings&,const std::optional<WorldMoment>& requested,ExactPhaseTechnicalBudget={},ExactArithmeticBudget={});
}

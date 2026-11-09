// C§21.1/R§4的唯一工作计量。专业配方、资源和成果仍由实际owner写，本层不生成通用成功。
#pragma once
#include "source_ref.hpp"
#include "hanmo/resident/world_moment.hpp"
#include <functional>
namespace hanmo::resident {
// ActorAction和原OwnerPeriod排他绑定；OwnerPeriod不接受任意领域字符串。
struct ActorActionWorkBinding { StableId action_id,action_kind; SourceRef source_owner_ref; };
enum class WorkPeriodOwner { local_economy,estate,organization };
struct OwnerPeriodWorkBinding { WorkPeriodOwner domain=WorkPeriodOwner::local_economy; StableId activity_kind,activity_id,registered_period_profile; };
using WorkExecutionBinding=std::variant<ActorActionWorkBinding,OwnerPeriodWorkBinding>;
// /1仍是原整数期间。精确经济工作窗由S5 EconomicWorkPeriodRead/2持有，不能借此截掉分数。
struct WorkPeriodRef { WorkExecutionBinding binding; Integer period_ordinal=0,from_tick=0,to_tick=0,source_schema_version=1; };
// 保既有0—4物理tag；正工期清单的真实满工待结算追加5，不重编号terminal。
enum class WorkExecutionPhase { working=0,suspended=1,awaiting_process_choice=2,awaiting_output_space=3,terminal=4,awaiting_settlement=5 };
// 唯一原工作量，嵌入ActionElapsed或原周期，绝不能再给Job保存一个completed副本。
struct WorkExecutionState { StableId work_unit_id; Integer total_work_units=0,completed_work_units=0; std::optional<WorkPeriodRef> last_processed_period; WorkExecutionPhase phase=WorkExecutionPhase::working; StableId running_policy_ref; };
struct ActualActorWorkRole { StableId actor_id,role; };
enum class WorkPeriodDisposition { provided,interrupted,no_work };
// 深值只读凭证，引用原真实参与/劳力分配/资源/用力源。一个Actor在本期只算一次。
struct WorkPeriodView { WorkPeriodRef ref; std::vector<ActualActorWorkRole> actual_actor_roles; std::vector<StableId> actual_labor_allocations,facility_claim_refs; StableId work_unit_id; Integer units_before=0,units_added=0,units_after=0; std::vector<SourceRef> resource_effect_refs,exertion_source_refs; WorkPeriodDisposition disposition=WorkPeriodDisposition::no_work; std::optional<SourceRef> reason; };
// 接口由原owner实现，读取必须核其真实已应用结果，不能接受caller自报WorkPeriodView。
class WorkPeriodSourceRead {
public:
    virtual ~WorkPeriodSourceRead()=default;
    virtual Outcome<WorkPeriodView> read_work_period(const WorkPeriodRef&) const=0;
    virtual Status validate_actor_role(const WorkPeriodRef&,const ActualActorWorkRole&) const=0;
    virtual Status validate_labor_allocation(const WorkPeriodRef&,const StableId&) const=0;
    virtual Status validate_facility_service(const WorkPeriodRef&,const StableId&) const=0;
    virtual Status validate_resource_effect(const WorkPeriodRef&,const SourceRef&) const=0;
    virtual Status validate_exertion_source(const WorkPeriodRef&,const SourceRef&) const=0;
    virtual Status validate_stage_result(const WorkPeriodRef&,const SourceRef&) const=0;
};
// 真实kind的静态注册。所有函数必须齐备；选了名字而没有source或真实writer时准入拒绝。
struct WorkExecutionRegistration { StableId domain,activity_kind,period_profile,work_unit_id; Integer source_schema=0; bool actor_action_required=true; };
Status validate_work_binding(const WorkExecutionBinding&);
bool same_work_binding(const WorkExecutionBinding&,const WorkExecutionBinding&);
bool same_work_period_ref(const WorkPeriodRef&,const WorkPeriodRef&);
Status validate_work_period_value(const WorkPeriodView&);
Status validate_work_execution_state(const WorkExecutionState&);
Outcome<WorkPeriodView> read_verified_work_period(const WorkPeriodRef&,const WorkPeriodSourceRead&);
// 纯候选后继；原owner已成功提交该期间全部资源/效果才可采用，不触发时间推进。
Outcome<WorkExecutionState> apply_verified_work_period(const WorkExecutionState&,const WorkPeriodRef&,const WorkPeriodSourceRead&);
// Craft完整半日gate的唯一余量。work_done只在WorkExecutionState，不在Job或gate重复保存。
enum class CraftPace { normal,rush };
struct CraftWorkGate { WorkExecutionBinding execution_binding; Integer gate_ordinal=0; WorldMoment gate_opened_at,last_accounted_at; WorkUnits earned_service_units; CraftPace pace_for_gate=CraftPace::normal; StableId accepted_pace_source_ref; Integer rush_units_committed=0; std::optional<StableId> last_closed_gate_result_ref; std::optional<Integer> pending_stage_ordinal; std::optional<StableId> terminal_result_ref; };
struct CraftWorkGateResult { StableId result_id; WorkExecutionBinding execution_binding; Integer gate_ordinal=0; std::vector<StableId> actual_service_source_refs; Integer work_units_before=0,work_units_added=0,work_units_after=0; std::vector<Integer> reached_stage_ordinals; std::vector<StableId> material_effect_refs,fee_receipt_refs,exertion_source_refs; };
// C3原整数gate不赚fraction；其source由原五kind完整worker/material/result验证后提供。
enum class OriginalHalfDayGateProfile { copy,infusion,recharge,maintain,appraise };
struct OriginalHalfDayGate { WorkExecutionBinding original_execution_binding; OriginalHalfDayGateProfile original_gate_profile_ref=OriginalHalfDayGateProfile::copy; std::optional<WorkPeriodRef> last_consumed_period_ref; Integer next_period_ordinal=0; };
struct OriginalHalfDayGateResult { WorkExecutionBinding execution_binding; WorkPeriodRef period_source_ref; Integer gate_ordinal=0,provided_work_units=0,completed_before=0,completed_after=0,processed_tick=0; std::vector<SourceRef> stage_result_refs; };
// life v1c§4.1唯一七profile；仅完整服务半日加N，药/术/作品绝不套Craft赶工3/4。
enum class LifeHalfDayStageGateProfile { medicine_batch,work_composition,copy_production,elixir,talisman,infusion,ritual_execution };
const char* life_half_day_stage_profile_name(LifeHalfDayStageGateProfile) noexcept;
const char* original_half_day_gate_profile_name(OriginalHalfDayGateProfile) noexcept;
// Time2 Life W原owner实际期间来源；不冒原整数WorkPeriodRef/1。
struct LifeWorkPeriodSourceRef { SourceRef source_ref;Integer source_schema=0; };
struct LifeHalfDayStageGateState { LifeHalfDayStageGateProfile gate_profile_ref=LifeHalfDayStageGateProfile::medicine_batch; SourceRef stage_rule_source_ref; WorkUnits earned_service_units; };
struct LifeHalfDayStageGateResult { LifeWorkPeriodSourceRef work_period_ref; Integer completed_halfday_ordinal=0; std::vector<Integer> completed_stage_ordinals; std::vector<SourceRef> original_stage_result_refs; };
using SharedWorkGate=std::variant<CraftWorkGate,OriginalHalfDayGate,LifeHalfDayStageGateState>;
struct SharedWorkElapsedState { StableId action_ref; WorkExecutionState execution; SharedWorkGate gate; };
using SharedWorkGateResult=std::variant<CraftWorkGateResult,OriginalHalfDayGateResult,LifeHalfDayStageGateResult>;
// 完整结果逐字段比较，不经JSON/BoundaryValue深复制。
bool same_shared_work_gate_result(const SharedWorkGateResult&,const SharedWorkGateResult&);
Status validate_life_half_day_stage_gate(const WorkExecutionState&,const LifeHalfDayStageGateState&,Integer units_per_half_day,ExactArithmeticBudget budget={});
Outcome<WorkUnits> prepare_life_half_day_stage_slice(const WorkExecutionState&,const LifeHalfDayStageGateState&,const MomentSpan&,Integer units_per_half_day,ExactArithmeticBudget budget={});
Outcome<Integer> life_half_day_stage_remaining(const WorkExecutionState&,const LifeHalfDayStageGateState&,Integer units_per_half_day,ExactArithmeticBudget budget={});
class LifeWorkPeriodSourceRead;
struct ClosedLifeStageGate { WorkExecutionState execution; LifeHalfDayStageGateState gate; };
Outcome<ClosedLifeStageGate> consume_life_half_day_stage_gate(const WorkExecutionState&,const LifeHalfDayStageGateState&,const LifeHalfDayStageGateResult&,const LifeWorkPeriodSourceRead&,WorkExecutionPhase after_phase,Integer units_per_half_day,ExactArithmeticBudget budget={});
// 片结果只带未发布计量；到gate必须调用专业stage consumer，不自动发货/收费/疲劳。
struct CraftGateSlice { WorkUnits earned_service_units; WorldMoment last_accounted_at; bool gate_due=false; };
Status validate_craft_work_gate(const WorkExecutionState&,const CraftWorkGate&,Integer units_per_half_day,ExactArithmeticBudget budget={});
Outcome<CraftGateSlice> prepare_craft_work_slice(const WorkExecutionState&,const CraftWorkGate&,const MomentSpan&,Integer units_per_half_day,ExactArithmeticBudget budget={});
Outcome<WorldMoment> craft_next_gate_due(const WorkExecutionState&,const CraftWorkGate&,Integer units_per_half_day,ExactArithmeticBudget budget={});
Outcome<Integer> craft_remaining_half_days(const WorkExecutionState&,const CraftWorkGate&,Integer units_per_half_day,ExactArithmeticBudget budget={});
Outcome<Integer> craft_gate_work_increment(const WorkExecutionState&,const CraftWorkGate&);
// 真gate结果与原owner处理完的阶段状态一起形成后继；技术失败不修改输入。
struct ClosedCraftGate { WorkExecutionState execution; CraftWorkGate gate; };
Outcome<ClosedCraftGate> close_craft_work_gate(const WorkExecutionState&,const CraftWorkGate&,const CraftWorkGateResult&,WorkExecutionPhase after_phase,std::optional<Integer> pending_stage,std::optional<StableId> terminal_result,Integer units_per_half_day,ExactArithmeticBudget budget={});
// 原owner已办合法Resume/工序输入后才调用；暂停间隔不供工，已闭门只开启下一ordinal一次。
Outcome<ClosedCraftGate> resume_craft_work_gate(const WorkExecutionState&,const CraftWorkGate&,const WorldMoment& actual_resume_at,Integer expected_gate_ordinal,const std::optional<StableId>& expected_last_closed,Integer units_per_half_day,ExactArithmeticBudget budget={});
// 已满且原必需工序/输出确实完成后封闭既有gate；不增加工作量、费用或质量。
Outcome<ClosedCraftGate> finish_craft_gate_wait(const WorkExecutionState&,const CraftWorkGate&,const StableId& original_terminal_result,Integer units_per_half_day,ExactArithmeticBudget budget={});
Outcome<ClosedCraftGate> change_craft_pace(const WorkExecutionState&,const CraftWorkGate&,CraftPace,const StableId& accepted_source,Integer expected_gate_ordinal,const std::optional<StableId>& expected_last_closed,Integer units_per_half_day,ExactArithmeticBudget budget={});
// OriginalHalfDayGate仍反读实际完整期间，不接受由S0钟界合成的period。
struct ClosedOriginalGate { WorkExecutionState execution; OriginalHalfDayGate gate; OriginalHalfDayGateResult result; };
Outcome<ClosedOriginalGate> consume_original_half_day_gate(const WorkExecutionState&,const OriginalHalfDayGate&,const OriginalHalfDayGateResult&,const WorkPeriodSourceRead&,Integer units_per_half_day);
}

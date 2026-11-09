// 显式Time2作者选择与实际compiled闭包；旧工程输入仅保自身已登记范围。
#pragma once
#include "world_timeline.hpp"
#include "exact_phase_graph.hpp"
#include "action_time_validation.hpp"
#include "resident/facility_claims/model.hpp"
#include "resident/facility_claims/facility_claim_runtime.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
inline constexpr const char* exact_complete_profile_id="rational_world_complete_v2";
inline constexpr const char* exact_rest_profile_id="rational_rest_first_chain_v1";
inline constexpr const char* exact_social_profile_id="rational_rest_supply_reports_v1";
struct ExactSupplyReportSubscription { StableId actor_id; std::vector<StableId> report_source_keys; };
// provider清单必需显式存在；完整选择由实际注册、静态因果图与原writer排他性一并核验。
struct ExactActionTimeSelection { StableId action_kind,profile_id;Integer version=0; };
struct CompleteExactTimeSelection { std::vector<ExactPhaseSelection> phase_providers;std::vector<ExactActionTimeSelection> action_time_profiles;FacilityClaimsCatalog facility_claims; };
// compiled结果只拥有有限选择值；全部callback从当次WorldBundle取得真实业务借用。
struct CompleteExactTimeRuntime { ExactPhaseGraph phase_graph;std::vector<ActionTimeRegistration> action_times;FacilityClaimsCatalog facility_claims;std::vector<FacilityHolderPhaseSelection> facility_holder_profiles; };
struct ExactTimeProfile { StableId profile_id; TimeScaleRef scale_ref; std::vector<StableId> macro_providers; std::vector<ExactSupplyReportSubscription> supply_report_subscriptions;std::optional<CompleteExactTimeSelection> complete; };
// 完整选择只准编译注册闭包，缺任何真实stage/kind/source在安装前拒绝；不沿用rest空scope。
bool exact_complete_selected(const ExactTimeProfile&);
// Original phase1 keeps G+64; phase2 owns G+96 command cuts; explicit
// phase3 owns G+128 original LongRest mouth adoption sources.
// phase4 选择 G+160、Condition/3 与原永久成长五根 /2 的同一应用链。
Outcome<Integer> read_complete_growth_phase_version(const ExactTimeProfile&);
Outcome<CompleteExactTimeSelection> decode_complete_exact_time_selection(const BoundaryValue& world_time);
Outcome<CompleteExactTimeRuntime> compose_complete_exact_time_profile(const ExactTimeProfile&,const std::vector<ExactPhaseRegistration>& compiled_providers,const std::vector<ActionTimeRegistration>& compiled_actions,const std::vector<StableId>& selected_legacy_writer_keys,const std::vector<FacilityHolderRegistration>& compiled_facility_holders);
// 唯一Social选中集合；未选择的旧首链不暗启知识后继。
bool exact_social_selected(const ExactTimeProfile&);
// 缺world_time保legacy；选中却缺字段/未知provider/未接kind一律明确拒绝，不默认降级。
Outcome<std::optional<ExactTimeProfile>> decode_exact_time_profile(const BoundaryValue& scenario);
// 显式新语义身份只登记真实新增选择；原Growth物理G+32由原段selector派生。
Status append_exact_time_modules(CatalogIdentity&,const ExactTimeProfile* selected=nullptr);
// 只此实际service/rest具备Fixed writer；wait不是新的时间Action，其他kind不可偷偷排队至整界。
Status exact_time_action_allowed(const ExactTimeProfile&,const StableId& kind,const StableId& activity);
}

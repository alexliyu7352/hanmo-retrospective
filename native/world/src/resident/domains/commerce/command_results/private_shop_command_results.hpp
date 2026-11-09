// Shop1原事实与真实提交结果的永久对应；无新钱账，不给旧来源补造历史result。
#pragma once
#include "resident/domains/commerce/private_shop.hpp"
namespace hanmo::resident {
class ProjectionWriter;
inline constexpr const char* private_shop_command_results_module="private_shop_command_results";
inline constexpr const char* private_shop_command_results_capability="private_shop_command_results_v1";
struct FamilyState;struct FamilyLegacyState;struct TaskShopOrderState;struct PrivateShopLiquidationState;
struct PrivateShopCommandResultsCatalog {Integer version=1;};
// 仅由同一不可变原目录派生的提交权限；不是存档字段，不能由绑定行自行选择模式。
// 旧Resident多Actor接口与原dynamic_subjects本人接口分别保持自己的真实准入语义。
enum class PrivateShopSubmissionAuthority { legacy_actor_api,controlled_player };
// 一个外层真实提交可以包含命令业务与多个NPC/半日派生事实；submitter是原实际请求Actor。
// 是否必须受控player由原目录入口决定，店员/客户不改主体；每个source仍由原Shop全史证明。
struct PrivateShopCommandResultBinding {
    StableId id,source_kind,submitter_actor_id,result_id,result_kind;
    Integer source_tick=0,from_tick=0,result_tick=0;
    Integer allocation_begin=0,request_seq=0,revision=0;
};
struct PrivateShopCommandResultsState {
    Integer version=1;
    OrderedTable<PrivateShopCommandResultBinding> bindings;
    // 原控制事实只保存真正提交对应；控制许可/人物链始终由原Family账持有。
    OrderedTable<PrivateShopCommandResultBinding> control_bindings;
};
// 完整Shop/Task/LE/市场与身份史先验之后调用；禁用时不迁移或补绑定旧Shop1。
Status validate_private_shop_command_results_pair(bool enabled,bool private_shops_enabled,const std::vector<std::pair<StableId,StableId>>& modules);
Status validate_private_shop_command_results(const CommonStore&,const PrivateShopState&,const PrivateShopCommandResultsState*,const FamilyState* family=nullptr,const FamilyLegacyState* legacy=nullptr,ProjectionWriter* projection=nullptr);
// 原Task/清算完整账后交叉零时业务与其真实主体/终结原因/既有actualresult。
Status validate_private_shop_command_related_sources(const PrivateShopState&,const PrivateShopCommandResultsState*,const TaskShopOrderState*,const PrivateShopLiquidationState*);
// 此唯一writer必须在原外层before/after与实际Common cache/history已建立后调用。
// 不开第二Common保存点，不把NPC即时返回冒独立玩家result；失败交原整候选回滚。
Status bind_private_shop_command_results_in_candidate(
    const CommonStore& before_common,const PrivateShopState& before_shops,const PrivateShopCommandResultsState& before,
    CommonStore& common,const PrivateShopState& shops,PrivateShopCommandResultsState& state,
    const StableId& submitter,Integer request_seq,const CommitResult& actual_result,
    const FamilyState* before_family=nullptr,const FamilyLegacyState* before_legacy=nullptr,
    const FamilyState* family=nullptr,const FamilyLegacyState* legacy=nullptr,
    PrivateShopSubmissionAuthority authority=PrivateShopSubmissionAuthority::controlled_player);
}

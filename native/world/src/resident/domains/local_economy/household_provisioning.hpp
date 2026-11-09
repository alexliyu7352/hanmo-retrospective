// 家庭自养只授权同一家庭私粮与既有供养周期；不保存粮食余额，也不授予官员私人产权。
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
struct LocalEconomyCatalog;
struct LocalEconomyState;
struct RecurringObligation;
struct LocalEconomyEffects;
struct LaborAllocation;

// 作者明确指出原家庭、群体、义务、唯一供养绑定和私粮容器；启用时另生成真实来源事实。
struct HouseholdProvisionAuthorization {
    StableId id,household_group_id,cohort_id,obligation_id,provision_binding_id;
    StableId inventory_id,item_id,rate_profile_id;
};
struct HouseholdProvisionCatalog {
    Integer version=1;
    OrderedTable<HouseholdProvisionAuthorization> authorizations;
};
// 登记与终止只保存来源身份；原RationReceipt仍是唯一每期消费结果。
struct HouseholdProvisionRegistration {StableId id,authorization_id;Integer tick=0;};
struct HouseholdProvisionClosure {StableId id,registration_id,obligation_id;Integer tick=0;};
struct HouseholdProvisionState {
    Integer version=1;
    OrderedTable<HouseholdProvisionRegistration> registrations;
    OrderedTable<HouseholdProvisionClosure> closures;
};

// 校验实际家庭产权、同群体义务与原供养区间，不接受组织grant作为私粮授权。
Status validate_household_provision_catalog(const CommonStore&,const LocalEconomyCatalog&,const HouseholdProvisionCatalog&);
// 初始化仅登记已验证作者来源，不扣粮、不结算历史周期。
Outcome<HouseholdProvisionState> make_initial_household_provision(CommonStore&,const LocalEconomyCatalog&,const HouseholdProvisionCatalog&);
// 冷态须与真实事实头、作者授权和不可变原周期对应；缺事实不能补造默认授权。
Status validate_household_provision_state(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const HouseholdProvisionCatalog&,const HouseholdProvisionState&);
// 此只读检查用于原ration结算点；每次都重核容器产权和到期覆盖，不替代原资源recipe。
Status check_household_ration_authority(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const HouseholdProvisionCatalog&,const HouseholdProvisionState&,const RecurringObligation&,Integer due_tick);
// 到期只关闭后续自养，调用者的阶段保存点覆盖事实与义务状态。
Status close_expired_household_provision(CommonStore&,LocalEconomyState&,const LocalEconomyCatalog&,Integer tick,LocalEconomyEffects&);
// 仅新自养群体采用有限收据覆盖；nullopt表示沿既有其他供养规则处理。
Outcome<std::optional<Integer>> household_provision_workers(const LocalEconomyState&,const LocalEconomyCatalog&,const LaborAllocation&,Integer due_tick,std::vector<StableId>& causes);
}

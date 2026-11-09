// 原Growth热入口借用的显式康复服务；每次由同World候选在栈内构造，不能跨clone保存。
#pragma once
#include "growth_recovery_rules.hpp"
#include "growth_recovery_qualification.hpp"
namespace hanmo::resident {
class GrowthPersonalRecoveryServices {
public:
    virtual ~GrowthPersonalRecoveryServices()=default;
    virtual Status preview(const CommonStore&,const StableId& actor,const GrowthQuote&) const=0;
    virtual Outcome<GrowthQuote> preview_cancel(const CommonStore&,const GrowthState&,const StableId&,const GrowthCommand&) const=0;
    virtual Outcome<GrowthEffect> cancel(CommonStore&,GrowthState&,const StableId&,const GrowthQuote&)=0;
    virtual Status started(CommonStore&,GrowthState&,const ActionCoreView&)=0;
    virtual Status provided(CommonStore&,GrowthState&,const ActionCoreView&)=0;
    virtual Status completed(CommonStore&,GrowthState&,const ActionCoreView&)=0;
};
// 调用者负责Common/Growth/Recovery整组保存点；phase仅在原actions_step动态栈内借用。
class BoundGrowthPersonalRecoveryServices final:public GrowthPersonalRecoveryServices {
public:
    BoundGrowthPersonalRecoveryServices(const CommonCatalog&,const GrowthCatalog&,const GrowthRecoveryCatalog&,GrowthRecoveryState&,const GrowthRecoveryHistoryAccess&,const GrowthRecoveryOccupationRead&,const DynamicStepInput* phase=nullptr);
    Status preview(const CommonStore&,const StableId&,const GrowthQuote&) const override;
    Outcome<GrowthQuote> preview_cancel(const CommonStore&,const GrowthState&,const StableId&,const GrowthCommand&) const override;
    Outcome<GrowthEffect> cancel(CommonStore&,GrowthState&,const StableId&,const GrowthQuote&) override;
    Status started(CommonStore&,GrowthState&,const ActionCoreView&) override;
    Status provided(CommonStore&,GrowthState&,const ActionCoreView&) override;
    Status completed(CommonStore&,GrowthState&,const ActionCoreView&) override;
private:
    const CommonCatalog& common_;const GrowthCatalog& growth_;const GrowthRecoveryCatalog& catalog_;GrowthRecoveryState& state_;const GrowthRecoveryHistoryAccess& history_;const GrowthRecoveryOccupationRead& occupation_;const DynamicStepInput* phase_;
};
// 通用释放前先以真实取消/死亡终止原Life并记康复终态；不释放共同占用/钱包，不重复结算。
Status stop_growth_recovery_service_in_candidate(CommonStore&,GrowthState&,const GrowthRecoveryCatalog&,GrowthRecoveryState&,const GrowthRecoveryHistoryAccess&,const StableId& action_id,const StableId& reason,const StableId& actual_cause={},const DynamicStepInput* phase=nullptr);
}

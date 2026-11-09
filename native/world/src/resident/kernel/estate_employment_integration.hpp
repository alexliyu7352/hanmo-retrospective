// 同World的雇用借口只保活原Estate/Interaction服务；不复制人物、工资或预约资产。
#pragma once
#include "resident/domains/estate/employment/estate_employment_boundary.hpp"
#include "resident/domains/estate/employment/estate_employment_profile.hpp"
namespace hanmo::resident {
struct WorldBundle;
class EstateEmploymentWorldAccess final {
public:
    // 仅在显式enabled且目录完整时构造；phase由原半日处理器给出。
    explicit EstateEmploymentWorldAccess(const WorldBundle&,const DynamicStepInput* phase=nullptr);
    EstateEmploymentWorldAccess(const EstateEmploymentWorldAccess&)=delete;
    EstateEmploymentWorldAccess& operator=(const EstateEmploymentWorldAccess&)=delete;
    const EstateEmploymentContext& context()const{return context_;}
    const EstateEmploymentMeetingContext& meeting()const{return meeting_;}
private:
    EstateAccess estate_;InteractionAccess interaction_;
    EstateEmploymentContext context_;EstateEmploymentMeetingContext meeting_;
};
Status validate_estate_employment_world(const WorldBundle&);
Outcome<EstateEmploymentQuote> prepare_estate_employment_world(const WorldBundle&,const StableId&,const EstateEmploymentCommand&);
Outcome<EstateReceipt> commit_estate_employment_world(WorldBundle&,const StableId&,const EstateEmploymentCommand&,const EstateEmploymentQuote&);
Outcome<BoundaryValue> query_estate_employment_world(const WorldBundle&,Integer cursor,Integer policy_cursor);
Status step_estate_employment_world(WorldBundle&,const DynamicStepInput&);
Status finish_estate_employment_world(WorldBundle&);
Status guard_estate_employment_world_management(const WorldBundle&,const EstateCommand&);
}

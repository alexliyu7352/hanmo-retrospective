// 运输费用的隔离来源合同：仅原Travel实际写者可提供，当前没有生产实现或公共profile。
#pragma once
#include "resident/domains/travel/travel_model.hpp"
namespace hanmo::resident {
// 原实际起行或改道之后的完整typed计划；不得从今日活动参数恢复已经走过的计划。
struct PrivateShopTravelExpensePlanSource {
    StableId fact_id,action_id,actor_id,payer_wallet_id,previous_plan_fact_id;
    Integer tick=0;
    bool replanned=false;
    Location original_location;
    std::variant<TerrainTravelParams,RoadTravelParams> actual_plan;
};
// 原同一成功step的实际收费。免费端点收尾也有step源，但不得生成付款源或金额。
// paid_amount由原收费recipe后的实际结果提供，不能按到达时间、最终位移或当前余额猜算。
struct PrivateShopTravelExpenseStepSource {
    StableId fact_id,action_id,plan_fact_id,payer_wallet_id,payment_fact_id;
    Integer from_tick=0,to_tick=0,paid_amount=0;
    bool free_endpoint_finalization=false;
    Location before_location,after_location;
};
// 普通路遇是原Interaction EncounterRecord而非Common fact，必须由原retained引用永久保留。
// 军事/舟战来源在完整适配前不可冒已支持普通运输；这里仅区分真实原源的种类。
enum class PrivateShopTravelContactSourceKind {encounter,military_contact,naval_contact};
// 原接敌截停只引用已执行的同半日step；位置可被截断，原费用不得重记、退款或覆盖。
struct PrivateShopTravelExpenseContactSource {
    StableId fact_id,action_id,step_fact_id,original_contact_source_id;
    PrivateShopTravelContactSourceKind original_contact_source_kind=PrivateShopTravelContactSourceKind::encounter;
    Integer tick=0;
    Location before_truncation,contact_location;
};
// 暂停/等待/复行是无付款的控制源；实际TravelWait可推进其他人，不表示本行动实际移动。
enum class PrivateShopTravelExpenseControlKind {pause,wait,resume};
struct PrivateShopTravelExpenseControlSource {
    StableId fact_id,action_id,plan_fact_id;
    Integer tick=0;
    PrivateShopTravelExpenseControlKind kind=PrivateShopTravelExpenseControlKind::pause;
};
// 只有原具名writer可结束费用链；generic erase、今日Action缺失或任意reason字串不在此集合。
enum class PrivateShopTravelExpenseEndKind {step_completed,zero_distance_control,rescue_replacement,military_casualties,naval_round,naval_capture,naval_continue,naval_navigation};
// step/control引用本链真实执行源，军战引用原唯一业务头，救助引用实际改kind来源。
// rescue_replacement只终止旧Travel许可；原finish_rescue后续货损/返起点须另核，不是到店。
// 能否入库还须另外证明指定货批完整连续性；本合同仍未注册且没有持久writer。
struct PrivateShopTravelExpenseEndSource {
    StableId fact_id,action_id,last_plan_fact_id,last_step_fact_id,original_end_source_id;
    PrivateShopTravelExpenseEndKind original_end_kind=PrivateShopTravelExpenseEndKind::step_completed;
    Integer tick=0;
    Location actual_location;
};
using PrivateShopTravelExpenseEvent=std::variant<PrivateShopTravelExpensePlanSource,
    PrivateShopTravelExpenseStepSource,PrivateShopTravelExpenseContactSource,
    PrivateShopTravelExpenseControlSource,PrivateShopTravelExpenseEndSource>;
// 只允许原具名携运委托与实际到店入库边界；任意旅行action不能被追认成商用费用。
struct PrivateShopTravelExpenseRequest {
    StableId consignment_fact_id,carrier_actor_id,payer_wallet_id,action_id;
    StableId actual_arrival_fact_id;
    Integer arrival_tick=0;
};
struct PrivateShopTravelExpenseSource {
    WorldIdentity source_identity;
    PrivateShopTravelExpenseRequest request;
    // 实际Common顺序的完整原事件，含零收费step；不能靠只列付款行掩盖漏步/换计划。
    std::vector<PrivateShopTravelExpenseEvent> events;
};
class PrivateShopOriginalTravelExpenseRead {
public:
    virtual ~PrivateShopOriginalTravelExpenseRead()=default;
    // 实现须先完整核原起行/改道/实付step/截停及终结来源，按原写者反向覆盖；不回调完整Shop。
    // 无原完整来源返回error，不能把无账或当前活动结束解释为零运费。
    virtual Outcome<PrivateShopTravelExpenseSource> private_shop_original_travel_expenses(
        const PrivateShopTravelExpenseRequest&)const=0;
};
}

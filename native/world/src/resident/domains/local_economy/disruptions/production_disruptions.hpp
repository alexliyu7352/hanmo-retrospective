// 明确作者来源的有限减产；本域不拥有库存，也不重复扣除原生产投入。
#pragma once
#include "../local_economy_model.hpp"
namespace hanmo::resident {
inline constexpr const char* production_disruption_profile="authored_production_disruptions_v1";
inline constexpr const char* production_disruption_combination="minimum_remaining_yield_v1";
struct ProductionDisruptionCause {
    StableId id,source_key,site_id,reason_key;
    Integer from_tick=0,through_tick=0,remaining_yield_bp=10000;
    std::vector<StableId> unit_ids;
};
struct ProductionDisruptionCatalog {
    Integer version=1;
    StableId profile=production_disruption_profile,combination_policy=production_disruption_combination;
    OrderedTable<ProductionDisruptionCause> causes;
};
// 登记只证明作者计划，未来减产是否发生由实际到期周期决定。
struct ProductionDisruptionRegistration {StableId id,cause_id;Integer tick=0;};
struct ProductionDisruptionOutput {
    StableId inventory_id,item_id;
    Integer original_potential=0,unproduced=0,adjusted_potential=0,stored=0,capacity_shortfall=0;
};
struct ProductionDisruptionSettlement {
    StableId id,production_receipt_id,unit_id;
    Integer tick=0,due_tick=0,remaining_yield_bp=10000;
    std::vector<StableId> registration_fact_ids;
    std::vector<ProductionDisruptionOutput> outputs;
};
struct ProductionDisruptionState {
    Integer version=1;
    OrderedTable<ProductionDisruptionRegistration> registrations;
    OrderedTable<ProductionDisruptionSettlement> settlements;
};
struct ProductionDisruptionQuote {
    StableId unit_id;Integer tick=0,due_tick=0,remaining_yield_bp=10000;
    std::vector<StableId> registration_fact_ids;
};
// 只在共同状态与经济同一候选内写入，不新增阶段或时钟。
struct ProductionDisruptionContext {
    const ProductionDisruptionCatalog& catalog;
    ProductionDisruptionState& state;
};
Status validate_production_disruption_catalog(const LocalEconomyCatalog&,const ProductionDisruptionCatalog&);
Outcome<ProductionDisruptionState> make_initial_production_disruptions(CommonStore&,const LocalEconomyCatalog&,const ProductionDisruptionCatalog&);
Outcome<ProductionDisruptionState> make_initial_production_disruptions_in_candidate(CommonStore&,const LocalEconomyCatalog&,const ProductionDisruptionCatalog&);
Outcome<ProductionDisruptionQuote> quote_production_disruption(const CommonStore&,const LocalEconomyCatalog&,const ProductionDisruptionCatalog&,const ProductionDisruptionState&,const ProductionUnit&,Integer tick,Integer due_tick);
// 本期整数潜在产出独立向下取整；届满后不补偿前期损失。
Outcome<Integer> disrupted_production_quantity(Integer original_potential,Integer remaining_yield_bp);
// 原ProductionReceipt已生成后关联来源；不改写原收据或再次消耗投入。
Outcome<StableId> record_production_disruption(CommonStore&,ProductionDisruptionState&,const ProductionDisruptionQuote&,const ProductionReceipt&,const std::vector<ProductionQuantity>& original_potential);
Status validate_production_disruptions(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const ProductionDisruptionCatalog&,const ProductionDisruptionState&);
// 供内部事实读服务复用，公开可见性仍由合法生产单元与历史索引决定。
std::optional<DynamicFactView> production_disruption_dynamic_fact(const ProductionDisruptionState&,const StableId&);
// 后继动态原因须由具名事件事实及独立版本来源验证器提供；
// 任意Common事实ID或作者字符串不能授权激活。
struct ProductionDisruptionOccurrence {
    WorldIdentity source_identity;StableId fact_id,source_domain,source_rule_id,site_id;
    Integer occurred_tick=0,from_tick=0,through_tick=0,remaining_yield_bp=10000;
    std::vector<StableId> unit_ids;
};
class ProductionDisruptionOccurrenceRead {
public:
    virtual ~ProductionDisruptionOccurrenceRead()=default;
    virtual Outcome<ProductionDisruptionOccurrence> production_disruption_occurrence(const StableId& fact_id)const=0;
};
}

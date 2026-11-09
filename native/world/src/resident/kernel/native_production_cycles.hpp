// 原 P01 生产和 Calendar 来源只持 production_cycles 一个根；具体 reader 借同候选拥有图。
#pragma once
#include "exact_complete_world.hpp"
#include "resident/domains/local_economy/economic_exact_phase.hpp"
namespace hanmo::resident {
Outcome<Integer> selected_native_production_cycles_version(const CatalogIdentity&);
// 原明示P02/2仍可读；production/3必需同一P02/3，缺失、重複及錯配拒絕。
Outcome<Integer>
selected_native_population_transport_version(const CatalogIdentity &);
Status append_native_production_cycles_module(const BoundaryValue& scenario,CatalogIdentity&);
Status prepare_native_production_cycles_catalog(const BoundaryValue& scenario,PreparedCatalog&);
Status validate_native_production_cycles_catalog(const PreparedCatalog&);
Status validate_native_production_cycles_layout(const WorldBundle&);
// 原头只关联唯一 P01 拥有行；真实时间、输入和履约仍由原全验门检查。
bool is_native_production_cycle_fact(const FactRecord&)noexcept;
Status validate_native_production_cycle_fact(const EconomicWorkSourcesState&,const FactRecord&);
class NativeProductionCyclesRead final:public EconomicExactReadLease,public CheckpointAllocatedObject {
    std::unique_ptr<CompleteExactWorldBindingOwner> owner_;
    std::shared_ptr<const EconomicExactReadLease> original_;
    NativeProductionCyclesRead(std::unique_ptr<CompleteExactWorldBindingOwner>,std::shared_ptr<const EconomicExactReadLease>);
public:
    // 两层 lease 同时保活，不能把本栈 bindings 引用交给下一候选或缓存。
    static Outcome<std::unique_ptr<NativeProductionCyclesRead>> create(const WorldBundle&);
    NativeProductionCyclesRead(const NativeProductionCyclesRead&)=delete;
    NativeProductionCyclesRead& operator=(const NativeProductionCyclesRead&)=delete;
    EconomicExactReadBinding bindings()const override{return original_->bindings();}
};
// compiled 阶段外层只转实际拥有工厂；实际 owner 不能将此转发器填回 economics 形成递归。
EconomicExactWorldFactory native_production_cycles_factory();
Status initialize_native_production_cycles_in_candidate(WorldBundle&);
Status validate_native_production_cycles_world(const WorldBundle&);
}

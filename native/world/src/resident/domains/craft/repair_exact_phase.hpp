// Repair is folded into the one original crafting provider, never a second clock.
#pragma once
#include "repair_contract.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
struct RepairExactReadBinding {const RepairReadContext& repair;};
struct RepairExactWriteBinding {CommonStore& common;ItemInstancesState& items;ActionElapsedStore& elapsed;const RepairWriteContext& repair;};
class RepairExactReadLease {public:virtual ~RepairExactReadLease()=default;virtual RepairExactReadBinding bindings()const=0;};
class RepairExactWriteLease {public:virtual ~RepairExactWriteLease()=default;virtual RepairExactWriteBinding bindings()=0;};
struct RepairExactWorldFactory {
 Outcome<std::shared_ptr<const RepairExactReadLease>> (*read)(const WorldBundle&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<RepairExactWriteLease>> (*write)(WorldBundle&,const WorldMoment&,ExactPhaseWriteContext&)=nullptr;
};
// Compiled central callbacks acquire the original same-world domain lease.
RepairExactWorldFactory native_repair_exact_world_factory() noexcept;
bool repair_exact_selected(const RepairExactWorldFactory&)noexcept;
Status validate_repair_exact_author_selection(const RepairExactWorldFactory&,const WorldBundle&);
Status validate_repair_exact_world(const RepairExactWorldFactory&,const WorldBundle&,const WorldMoment&);
Outcome<std::vector<ExactEventPrediction>> predict_repair_exact_events(const RepairExactWorldFactory&,const WorldBundle&,const WorldMoment&,const WorldMoment&);
Status provide_repair_exact_interval(const RepairExactWorldFactory&,WorldBundle&,const MomentSpan&,ExactPhaseWriteContext&);
Outcome<ExactEventApplied> apply_repair_exact_due(const RepairExactWorldFactory&,WorldBundle&,const ExactEventPrediction&,ExactPhaseWriteContext&);
Status validate_repair_exact_applied(const RepairExactWorldFactory&,const WorldBundle&,const ExactEventApplied&);
}

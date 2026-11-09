// Original Craft Job service, gate and output consumers in the exact causal graph.
#pragma once
#include "craft_interval.hpp"
#include "craft_output.hpp"
#include "repair_exact_phase.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
struct CraftExactReadBinding {const CraftJobReadContext& craft;};
struct CraftExactWriteBinding {
 CommonStore& common;CraftJobsState& jobs;ActionElapsedStore& elapsed;
 ItemInstancesState& instances;LotPropertiesState& properties;
 const CraftJobWriteContext& craft;const ItemOutputSourceRead& item_outputs;
};
class CraftExactReadLease {public:virtual ~CraftExactReadLease()=default;virtual CraftExactReadBinding bindings()const=0;};
class CraftExactWriteLease {public:virtual ~CraftExactWriteLease()=default;virtual CraftExactWriteBinding bindings()=0;};
// A write lease borrows this candidate's capture collection. No world pointers
// or source adapters are retained in the compiled callback itself.
struct CraftExactWorldFactory {
 Outcome<std::shared_ptr<const CraftExactReadLease>> (*read)(const WorldBundle&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<CraftExactWriteLease>> (*write)(WorldBundle&,const WorldMoment&,ExactPhaseWriteContext&)=nullptr;
};
// Compiled central callbacks acquire the original same-world domain lease.
CraftExactWorldFactory native_craft_exact_world_factory() noexcept;
Outcome<std::vector<ExactEventPrediction>> predict_craft_exact_events(const CraftJobReadContext&,const WorldMoment& from,const WorldMoment& until);
ExactPhaseRegistration make_craft_exact_phase_registration(CraftExactWorldFactory);
ExactPhaseRegistration make_craft_exact_phase_registration(CraftExactWorldFactory,RepairExactWorldFactory);
}

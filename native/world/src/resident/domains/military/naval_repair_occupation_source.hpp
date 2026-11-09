#pragma once
#include "naval_repair_source_local.hpp"
namespace hanmo::resident {
// Borrow the one accepted terms/participant vector and real original end row.
// This proves an execution's identity and adoption cuts, not service eligibility.
struct NavalRepairOccupationSourceView {
 const NewNavalRepairTimeSource2* original=nullptr;
 const ActionElapsedState* elapsed=nullptr;
 const FixedServiceTransition* started_transition=nullptr;
 const FixedServiceTransition* terminal_transition=nullptr;
};
class NativeNavalRepairOccupationSourceRead {
 const CommonStore& common_;const NavalState& naval_;
 const MilitaryActionTimeState& state_;const ActionElapsedStore& elapsed_;
 ExactNavalRepairSourceContext context_;bool selected_;
public:
 // selected is the original trusted catalogue's exact NavalRepair choice.
 // A stored row or non-null provider does not select this branch.
 NativeNavalRepairOccupationSourceRead(const CommonStore& c,const NavalState& n,
  const MilitaryActionTimeState& s,const ActionElapsedStore& e,
  ExactNavalRepairSourceContext context,bool selected)
  :common_(c),naval_(n),state_(s),elapsed_(e),context_(context),selected_(selected){}
 bool bound_to(const CommonStore& c,const NavalState& n,const MilitaryActionTimeState& s,
  const ActionElapsedStore& e)const noexcept{return &c==&common_&&&n==&naval_&&&s==&state_&&&e==&elapsed_;}
 Outcome<std::vector<StableId>> enumerate_original_occupation_sources()const;
 Outcome<NavalRepairOccupationSourceView> read_original_occupation_source(const StableId& action)const;
};
}

#pragma once
#include "travel_dependent_state.hpp"
#include "resident/domains/family/born_character_source.hpp"
namespace hanmo::resident {
class NativeCareDependentPlacementRead;
// Original spatial adapters own the five circumstance proofs. The native
// adopting writer below never substitutes a Location tag for those proofs.
// Historical reads validate at the real birth cut, before the S1 result exists.
class BirthPlacementOriginalRead {
public:
 virtual ~BirthPlacementOriginalRead()=default;
 virtual bool bound_to(const CommonStore&,const MovementExactState&)const noexcept=0;
 virtual Status require_kind(BirthPlacementKind)const=0;
 virtual Outcome<BirthPlacement> read_actual(const BirthPlacementQuery&)const=0;
 virtual Status validate_at_actual_birth(const BirthPlacementAdoption&)const=0;
 // The original context determines whether a real dependent link is required;
 // no new custody/transport authority is inferred from a Location alternative.
 virtual Outcome<std::optional<MomentOwnerSource>> read_actual_transport_source(const BirthPlacementAdoption&)const=0;
 virtual Outcome<bool> requires_dependent_binding(const BirthPlacementAdoption&)const=0;
};
struct NativeBirthPlacementContext {
 CommonStore& common;MovementExactState& movement;TravelDependentState& dependents;
 const BornCharacterSourceRead& births;const BirthPlacementOriginalRead& original;
 const CurrentMomentRead& clock;const ReadStampAuthority& stamps;const DomainReadVersionSource& versions;
 Integer selected_version=2;ExactArithmeticBudget budget;
 const NativeCareDependentPlacementRead* care_handovers=nullptr;
};
class NativeBirthPlacementWrite final:public BirthPlacementWrite {
 NativeBirthPlacementContext& context_;
public:
 explicit NativeBirthPlacementWrite(NativeBirthPlacementContext& x):context_(x){}
 bool bound_to(const CommonStore&)const noexcept override;
 Status require_birth_placement_kind(BirthPlacementKind)const override;
 Outcome<BirthPlacement> read_birth_placement(const BirthPlacementQuery&)const override;
 Outcome<BirthPlacementApplicationRead> read_birth_placement_application(const MomentOwnerSource&,const MomentOwnerSource&)const override;
 Outcome<BirthPlacementApplicationRead> read_birth_placement_result(const MomentOwnerSource&)const override;
 Outcome<BirthPlacementApplicationRead> read_birth_placement_for_child(const StableId&)const override;
 Status validate_birth_placement_at_birth(const StableId&,const StableId&,const MomentOwnerSource&,const WorldMoment&,const BirthPlacement&)const override;
 Outcome<BirthPlacementApplication> adopt_birth_placement_in_candidate(CommonStore&,const BirthPlacementAdoption&)override;
 Status validate_birth_placement_application(const BirthPlacementAdoption&,const MomentOwnerSource&,const std::optional<StableId>&)const override;
};
Status validate_travel_dependents(const NativeBirthPlacementContext&);
}

#pragma once
#include "material_aid.hpp"
namespace hanmo::resident {
// E20.5: exactly the frozen maximum creates the one late liability. Not a quote,
// guessed price, automatic cash transfer or re-use of the original principal.
struct MaterialAidCompensationBasis {
 const MaterialAidObligation* original_aid=nullptr;
 const TreatyBoundedCompensation* original_policy=nullptr;
 MomentOwnerSource original_resource_acceptance;
 SourceRef original_establishment;
 WorldMoment actual_deadline;
 Integer amount=0;
};
// No late-source argument is the actual hot boundary before publishing its
// result. Cold calls MUST supply the retained original late source; later paid
// receipts cannot rewrite that source's original unpaid predicate.
// This cut deliberately does not read the compensation debt or full P11.
Outcome<MaterialAidCompensationBasis> read_material_aid_compensation_basis(
 const MaterialAidReadContext&,const ObligationRef&,
 const std::optional<SourceRef>& actual_late_source={});
}

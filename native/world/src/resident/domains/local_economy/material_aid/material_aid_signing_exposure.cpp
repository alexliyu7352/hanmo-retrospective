#include "material_aid_signing_exposure.hpp"
#include "material_aid_internal.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {
Outcome<MaterialAidSigningExposure> read_material_aid_signing_exposure(const TreatyMaterialAidTerm&term){try{
 using namespace aid_detail;
 if(term.occurrences<=0)fail("MaterialAidFiniteOccurrenceCountRequired");
 MaterialAidSigningExposure result;result.occurrences=term.occurrences;
 // Reuse the exact original normalization and positive unit checks used by
 // actual S5 schedule holds and occurrence establishment, not a second formula.
 for(const auto&measure:original_measures(term))
  result.ordinary.push_back({measure.unit,measure.promised,multiply(measure.promised,term.occurrences)});
 if(const auto*policy=std::get_if<TreatyBoundedCompensation>(&term.late_policy)){
  if(policy->currency_id!="qian"||policy->maximum_amount<0)fail("MaterialAidCompensationMoneyMeasureRequired");
  result.bounded_compensation=MaterialAidSigningMeasure{BudgetMoneyQian{},policy->maximum_amount,multiply(policy->maximum_amount,term.occurrences)};
 }
 // Combining same-unit components for Grant validation must also fit the exact
 // original integer range even if each individually fits. Nothing is persisted.
 if(result.bounded_compensation)for(const auto&m:result.ordinary)if(std::holds_alternative<BudgetMoneyQian>(m.unit))
  add(m.all_occurrences,result.bounded_compensation->all_occurrences);
 return {true,std::move(result),{}};
 }catch(aid_detail::Failure&e){return {false,{},std::move(e.error)};}
 catch(const std::bad_alloc&){return {false,{},common_failure("MaterialAidSigningProjectionExhausted").error};}
 catch(const std::length_error&){return {false,{},common_failure("MaterialAidSigningProjectionExhausted").error};}}
}

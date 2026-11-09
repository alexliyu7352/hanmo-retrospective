#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/domains/actor_foundation/model.hpp"
#include "injury_attribute_reducer.hpp"
#include "resident/storage/common_store.hpp"
#include <type_traits>
namespace hanmo::resident {
Outcome<Integer> evaluate_original_attribute_points(const OriginalAttributePointInputs& in){
 if(in.base<1||in.base>100||in.permanent_growth<0||in.permanent_growth>5||
    in.permanent_injury_points<0||in.recoverable_injury_points<0)
  return {false,{},common_failure("OriginalAttributePointInputsInvalid").error};
 // Sum every positive term before subtracting the negative terms. The largest
 // positive sum is Integer::max + 105, which fits its unsigned counterpart.
 // This avoids signed overflow, including temporary_points == Integer::min,
 // without an intermediate clamp that would discard legitimate cancellation.
 using Unsigned=std::make_unsigned_t<Integer>;
 static_assert(std::is_integral_v<Integer>&&std::is_signed_v<Integer>);
 Unsigned value=static_cast<Unsigned>(in.base+in.permanent_growth);
 if(in.temporary_points>0)value+=static_cast<Unsigned>(in.temporary_points);
 const Unsigned losses[]={
  static_cast<Unsigned>(in.permanent_injury_points),
  static_cast<Unsigned>(in.recoverable_injury_points),
  in.temporary_points<0?Unsigned{0}-static_cast<Unsigned>(in.temporary_points):Unsigned{0}
 };
 for(const auto loss:losses){
  if(loss>=value)return {true,Integer{1},{}};
  value-=loss;
 }
 return {true,value>Unsigned{110}?Integer{110}:static_cast<Integer>(value),{}};
}
Outcome<ActorAttributes> evaluate_original_attribute_points(
 const ActorAttributes& base,const ActorAttributes& growth,const ActorAttributes& temporary,
 const ActorAttributes& permanent_injury,const ActorAttributes& recoverable_injury){
 ActorAttributes out;
 Integer ActorAttributes::* const fields[]={
  &ActorAttributes::administration,&ActorAttributes::charisma,&ActorAttributes::intellect,
  &ActorAttributes::leadership,&ActorAttributes::martial
 };
 for(std::size_t i=0;i<5;++i){
  const auto field=fields[i];
  auto value=evaluate_original_attribute_points({base.*field,growth.*field,temporary.*field,
                                                 permanent_injury.*field,recoverable_injury.*field});
  if(!value.ok)return {false,{},std::move(value.error)};
  out.*field=value.value;
 }
 return {true,out,{}};
}
Status validate_original_injury_attribute_reducer(
 const InjuryAttributeReduction& value,const CatalogIdentity& identity){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 6, "InjuryAttributeReduction", "validate_original_injury_attribute_reducer", &value);

 const auto& id=value.attribute_id;
 if(id!="administration"&&id!="charisma"&&id!="intellect"&&
    id!="leadership"&&id!="martial")
  return common_failure("InvalidInjuryAttributeId",id);
 if(value.amount<0)return common_failure("InvalidInjuryReductionAmount",id);
 if(identity.content_digest.empty()||
    value.reducer_ref.catalog_digest!=identity.content_digest)
  return common_failure("InjuryAttributeReducerIdentityMismatch",id);
 if(value.reducer_ref.id!="original_flat_attribute_points"||
    value.reducer_ref.version!=1||value.unit_id!="attribute_points")
  return common_failure("UnsupportedOriginalInjuryAttributeReducer",id,
                        ErrorKind::unsupported_capability);
 return common_success();
}
}

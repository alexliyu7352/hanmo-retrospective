// P11/1 source-only successor, after removal of unadmitted prepayment.
// One finite callback feeds the existing JSON, binary and projection archives.
#pragma once
#include "obligation_sources_owned_fields.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include <string_view>
#include "resident/kernel/exact_struct_codec.hpp"
#include <type_traits>
namespace hanmo::resident::obligation_source_fields {
template<class C,class T>Status field(C&,const char*,T&);
template<class C>struct Fields {C&c;template<class T>void operator()(const char*n,T&v){field(c,n,v);}};
template<class C>Status record(C&c,SourceRef&v){return exact_fields::source(c,v);}
template<class C>Status record(C&c,MomentOwnerSource&v){field(c,"source_ref",v.source_ref);exact_fields::schema(c,"source_schema",v.source_schema);return c.status();}
template<class C>Status record(C&c,WorldMoment&v){return exact_fields::moment(c,v);}
template<class C>Status record(C&c,ObligationRef&v){Fields<C>fields{c};obligation_ref_fields(fields,v);if(c.status().ok){const auto valid=validate_obligation_ref_shape(v);if(!valid.ok)return c.reject(valid.error.original.code,valid.error.original.path);}return c.status();}
template<class C>Status record(C&c,SubjectRef&v){field(c,"kind",v.kind);field(c,"id",v.id);return c.status();}
template<class C>Status record(C&c,DomainReadVersion&v){field(c,"domain",v.domain);field(c,"kind",v.kind);field(c,"id",v.id);field(c,"revision",v.revision);return c.status();}
template<class C>Status record(C&c,InvestmentContractRef&v){field(c,"contract_id",v.contract_id);field(c,"terms_revision",v.terms_revision);return c.status();}
template<class C>Status record(C&c,OriginalRecurringObligationSource&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,OriginalGovernanceWorkCharge&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,OriginalServiceTerminationPayable&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,OriginalOrderedEarnedCharge&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,OriginalShopOrderRefund&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,OriginalShopCancellationFee&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,OriginalCrewShareAccrual&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,OriginalCrewShareCorrection&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,OriginalMaterialAidCompensationPayable&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,OriginalMaterialAidPayable&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,OriginalTransportExpensePayable&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,OriginalRepairEarnedCharge&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,OriginalInvestmentPrincipal&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,OriginalJointFundingReturn&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,OriginalJointDistribution&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,EconomicObligationSourceBinding&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,EconomicPayableSourceBinding&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,EconomicPayableAdjustmentBinding&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,EconomicRecoveryObligationBinding&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,ObligationResponsibilityEpochRef&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,OriginalResponsibilitySource&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,ConsentedObligationNovation&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,ObligationExecutionWalletChange&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,ObligationReceiverContinuity&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,ObligationOriginalPoliticalTakeover&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,ObligationResponsibilityEpoch&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,PaymentResponsibilityBinding&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>Status record(C&c,LegacyResponsibilityPrefix&v){Fields<C> fields{c};obligation_source_owned_fields::fields(fields,v);return c.status();}
template<class C>struct RootFields {
 C&c;
 template<class T>void operator()(const char*name,T&v){
  field(c,name,v);
  if constexpr(std::is_same_v<T,Integer>)if(c.status().ok&&std::string_view(name)=="schema_version"&&v!=1&&v!=2)c.reject("UnsupportedEconomicObligationSourcesVersion");
 }
};
template<class C>Status record(C&c,EconomicObligationSourcesState&v){if(!c.reading()&&!economic_obligation_sources_format_valid(v))return c.reject("EconomicObligationSourceOutsideSelectedFormat");RootFields<C> fields{c};obligation_source_owned_fields::fields(fields,v);if(c.status().ok&&!economic_obligation_sources_format_valid(v))return c.reject("EconomicObligationSourceOutsideSelectedFormat");return c.status();}

template<class T>struct Vector:std::false_type{};template<class T,class A>struct Vector<std::vector<T,A>>:std::true_type{};
template<class T>struct Optional:std::false_type{};template<class T>struct Optional<std::optional<T>>:std::true_type{};
template<class T>struct Variant:std::false_type{};template<class...T>struct Variant<std::variant<T...>>:std::true_type{};
template<std::size_t I=0,class C,class T>Status select(C&c,T&v,std::size_t tag){if constexpr(I<std::variant_size_v<T>){if(tag==I){auto prepared=prepare_checkpoint_default_values<std::variant_alternative_t<I,T>>(c);if(!prepared.ok)return prepared;v.template emplace<I>();return c.status();}return select<I+1>(c,v,tag);}else return c.reject("InvalidEconomicObligationVariant");}
template<class T>struct Names;
template<>struct Names<SubjectKind>{inline static constexpr std::pair<SubjectKind,const char*>rows[]={{SubjectKind::actor,"actor"},{SubjectKind::organization,"organization"},{SubjectKind::cohort,"cohort"}};};
template<>struct Names<EconomicObligationScheduler>{inline static constexpr std::pair<EconomicObligationScheduler,const char*>rows[]={{EconomicObligationScheduler::recurring,"recurring"},{EconomicObligationScheduler::source_only,"source_only"}};};
template<>struct Names<EconomicPayableAdjustmentKind>{inline static constexpr std::pair<EconomicPayableAdjustmentKind,const char*>rows[]={{EconomicPayableAdjustmentKind::positive_increment,"positive_increment"},{EconomicPayableAdjustmentKind::unpaid_void,"unpaid_void"}};};
template<class C,class T>Status value(C&c,T&v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field("value",v);
 else if constexpr(std::is_same_v<T,std::uint64_t>)return c.unsigned_field("value",v);
 else if constexpr(Variant<T>::value){Integer tag=static_cast<Integer>(v.index());exact_fields::finite(c,"tag",tag,static_cast<Integer>(std::variant_size_v<T>)-1);if(!c.status().ok)return c.status();if(c.reading()){auto selected=select(c,v,static_cast<std::size_t>(tag));if(!selected.ok)return selected;}return std::visit([&](auto&row){return field(c,"payload",row);},v);}
 else return record(c,v);
}
template<class C,class T>Status field(C&c,const char*name,T&v){
 if(!c.status().ok)return c.status();
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,v);
 else if constexpr(std::is_same_v<T,std::uint64_t>)return c.unsigned_field(name,v);
 else if constexpr(std::is_enum_v<T>){StableId named;if(!c.reading()){bool found=false;for(const auto&row:Names<T>::rows)if(v==row.first){named=row.second;found=true;}if(!found)return c.reject("UnknownEconomicObligationEnum",name);}c.field(name,named);if(!c.status().ok)return c.status();for(const auto&row:Names<T>::rows)if(named==row.second){if(c.reading())v=row.first;return c.status();}return c.reject("UnknownEconomicObligationEnum",name);}
 else if constexpr(Vector<T>::value)return c.sequence(name,v,[](auto&out,auto&row){return value(out,row);});
 else if constexpr(Optional<T>::value)return c.optional(name,v,[](auto&out,auto&row){return value(out,row);});
 else return c.object(name,[&](auto&out){return value(out,v);});
}
}

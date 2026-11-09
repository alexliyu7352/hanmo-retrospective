#include "organization_result_projection2.hpp"
namespace hanmo::resident {namespace {
bool grant_kind(const StableId&kind){return kind=="organization_grant_issued"||kind=="organization_grant_amended"||kind=="organization_grant_revoked";}
template<class T>Outcome<T>fail(const char*code,const StableId&id,ErrorKind kind=ErrorKind::internal){return {false,{},common_failure(code,id,kind).error};}
}
bool native_organization_authority_result_kind(const FactRecord&header){return header.domain=="organization"&&(header.kind=="organization_office_changed"||grant_kind(header.kind));}
Outcome<WorldMoment> NativeOrganizationAuthorityResultRead::read_selected_source_moment(const MomentOwnerSource&source)const{
 const auto&ref=source.source_ref;auto now=source_.clock.read_organization_authority_now();if(!now.ok)return {false,{},now.error};auto current=compare_world_moments(now.value,source_.actual);if(!current.ok)return {false,{},current.error};if(current.value!=0)return fail<WorldMoment>("StaleOrganizationAuthorityResultRead",ref.id);
 if(ref.domain!="organization")return fail<WorldMoment>("OrganizationAuthorityResultSourceRequired",ref.id,ErrorKind::unsupported_capability);
 if(ref.kind=="organization_office_changed"){auto read=read_office_authority_change_source(source_.common,source_.organization,source_.catalog,ref,source.source_schema,source_.stamp,source_.transitions,source_.deaths,source_.clock);if(!read.ok)return {false,{},read.error};return {true,read.value.change->effective_moment,{}};}
 if(grant_kind(ref.kind)){auto read=read_grant_authority_change_source(source_.common,source_.organization,source_.catalog,ref,source.source_schema,source_.clock);if(!read.ok)return {false,{},read.error};return {true,read.value.change->effective_moment,{}};}
 return fail<WorldMoment>("OrganizationAuthorityResultSourceRequired",ref.id,ErrorKind::unsupported_capability);
}
Outcome<ExactOwnerFactRead> NativeOrganizationAuthorityResultRead::read_selected_fact(const FactRecord&header)const{
 if(!native_organization_authority_result_kind(header))return fail<ExactOwnerFactRead>("OrganizationAuthorityResultSourceRequired",header.id,ErrorKind::unsupported_capability);
 const auto*actual=source_.common.state().facts.find(header.id);if(!actual||actual->domain!=header.domain||actual->kind!=header.kind)return fail<ExactOwnerFactRead>("OrganizationAuthorityResultHeaderMismatch",header.id);
 const Integer schema=header.kind=="organization_office_changed"?2:1;MomentOwnerSource ref{{header.domain,header.kind,header.id},schema};auto at=read_selected_source_moment(ref);if(!at.ok)return {false,{},at.error};
 auto original=organization_dynamic_fact(source_.organization,header.id);if(!original||original->id!=header.id||original->kind!=header.kind||original->tick!=at.value.half_day_tick)return fail<ExactOwnerFactRead>("OrganizationAuthorityResultProjectionMismatch",header.id);
 // The original S6 projection is retained verbatim apart from its real time basis.
 // A change in one's authority is not itself a public report or knowledge receipt.
 auto&v=*original;DynamicFactViewV2 fact{v.id,v.kind,v.rule_version,at.value,OwnerFactMoment{ref.source_ref,schema},v.cause_fact_ids,v.affair_ids,v.participants,v.supply,v.obligation,v.subject_id,v.outcome,v.publicly_visible};ExactOwnerFactRead out;out.fact=std::move(fact);return {true,std::move(out),{}};
}
}

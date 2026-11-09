#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "repair_service_rules.hpp"
#include "repair_service_codec_archive.hpp"
#include "resident/domains/craft/repair_policy.hpp"
namespace hanmo::resident {
namespace {
Status invalid(const char*code,const StableId&id={}){return common_failure(code,id,ErrorKind::internal);}
Status fee_shape(const StableId&key,const RepairServiceFeePolicy&p){if(key.empty()||key!=p.policy_id||p.version<1||(p.trigger!=RepairServiceFeeTrigger::actual_start&&p.trigger!=RepairServiceFeeTrigger::repair_completed))return invalid("InvalidOriginalRepairFeePolicy",key);return common_success();}
Status custody_shape(const StableId&key,const RepairServiceCustodyPolicy&p){if(key.empty()||key!=p.policy_id||p.version<1||p.uncollected_fee_qian<0||p.uncollected_period_half_days<0||(p.uncollected_fee_qian>0&&p.uncollected_period_half_days<1))return invalid("InvalidOriginalRepairCustodyPolicy",key);return common_success();}
bool source(const MomentOwnerSource&r,const SourceRef&expected){return r.source_schema==1&&same_source_ref(r.source_ref,expected);}
}
SourceRef repair_fee_policy_source(const StableId&id){return {"commerce_catalog","repair_fee_policy",id};}
SourceRef repair_custody_policy_source(const StableId&id){return {"commerce_catalog","repair_custody_policy",id};}
Status validate_repair_service_rule_catalog(const RepairServiceRuleCatalog&c,const RepairPolicyCatalog&original){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 17, "RepairServiceRuleCatalog", "validate_repair_service_rule_catalog", &c);

 for(const auto&r:c.fee_policies.rows()){auto s=fee_shape(r.first,r.second);if(!s.ok)return s;}
 for(const auto&r:c.custody_policies.rows()){auto s=custody_shape(r.first,r.second);if(!s.ok)return s;}
 for(const auto&r:original.policies.rows()){const auto&p=r.second;if(r.first!=p.policy_id||p.version<1||p.fee_qian<0||!c.fee_policies.find(p.fee_policy_ref)||!c.custody_policies.find(p.custody_policy_ref))return invalid("OriginalRepairCommercialPolicyMappingRequired",r.first);}
 return common_success();
}
Outcome<const RepairServiceFeePolicy*> read_repair_fee_policy(const RepairServiceRuleCatalog&c,const MomentOwnerSource&r){
 if(!source(r,repair_fee_policy_source(r.source_ref.id)))return {false,nullptr,common_failure("UnsupportedRepairFeePolicySource",r.source_ref.id,ErrorKind::unsupported_capability).error};const auto*p=c.fee_policies.find(r.source_ref.id);if(!p)return {false,nullptr,invalid("OriginalRepairFeePolicyRequired",r.source_ref.id).error};auto s=fee_shape(r.source_ref.id,*p);return s.ok?Outcome<const RepairServiceFeePolicy*>{true,p,{}}:Outcome<const RepairServiceFeePolicy*>{false,nullptr,s.error};
}
Outcome<const RepairServiceCustodyPolicy*> read_repair_custody_policy(const RepairServiceRuleCatalog&c,const MomentOwnerSource&r){
 if(!source(r,repair_custody_policy_source(r.source_ref.id)))return {false,nullptr,common_failure("UnsupportedRepairCustodyPolicySource",r.source_ref.id,ErrorKind::unsupported_capability).error};const auto*p=c.custody_policies.find(r.source_ref.id);if(!p)return {false,nullptr,invalid("OriginalRepairCustodyPolicyRequired",r.source_ref.id).error};auto s=custody_shape(r.source_ref.id,*p);return s.ok?Outcome<const RepairServiceCustodyPolicy*>{true,p,{}}:Outcome<const RepairServiceCustodyPolicy*>{false,nullptr,s.error};
}
Status validate_repair_service_rule_terms(const RepairServiceRuleCatalog&c,const RepairPolicyCatalog&policies,const RepairServiceQuotedTerms&t){
 auto valid=validate_repair_service_rule_catalog(c,policies);if(!valid.ok)return valid;auto original=read_repair_policy(policies,t.repair_policy_ref);if(!original.ok)return {false,{},original.error};const auto&p=*original.value;
 auto fee=read_repair_fee_policy(c,t.fee_policy_source);if(!fee.ok)return {false,{},fee.error};auto custody=read_repair_custody_policy(c,t.custody_terms.original_rule_ref);if(!custody.ok)return {false,{},custody.error};
 if(fee.value->policy_id!=p.fee_policy_ref||!source(t.fee_terms.original_rule_ref,repair_fee_policy_source(p.fee_policy_ref))||t.fee_terms.trigger!=fee.value->trigger||t.fee_terms.amount_qian!=p.fee_qian||custody.value->policy_id!=p.custody_policy_ref||t.custody_terms.delivery_before_acceptance!=custody.value->delivery_before_acceptance||t.custody_terms.uncollected_fee_qian!=custody.value->uncollected_fee_qian||t.custody_terms.uncollected_period_half_days!=custody.value->uncollected_period_half_days)return invalid("RepairAcceptedOriginalPolicyMismatch",t.instance_id);
 // Return terms are instance-specific original accepted terms. A static
 // catalog row cannot stand for that agreement; its actual source is still
 // validated by the original commercial acceptance reader.
 if(t.return_terms.original_rule_ref.source_ref.domain=="commerce_catalog")return invalid("RepairAgreedTermsSourceRequired",t.instance_id);
 return common_success();
}
Outcome<RepairServiceRuleCatalog> decode_repair_service_rule_catalog(const BoundaryValue&v,const RepairPolicyCatalog&policies){try{if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<RepairServiceRuleCatalog>();RepairServiceRuleCatalog out;repair_service_codec::decode(v,out,"/repair_service_rule_catalog");auto s=validate_repair_service_rule_catalog(out,policies);if(!s.ok)return {false,{},s.error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}}
Outcome<BoundaryValue> encode_repair_service_rule_catalog(const RepairServiceRuleCatalog&v){return repair_service_codec::encode(v);}
Status emit_repair_service_rule_catalog(ProjectionWriter&w,const StableId&name,const RepairServiceRuleCatalog&v){return repair_service_codec::emit(w,name,v);}
Status visit_repair_service_rule_catalog(CodecVisitor&c,RepairServiceRuleCatalog&v){repair_service_codec::Visit f{c};repair_service_fields::fields(f,v);return f.result.ok?c.status():f.result;}
}

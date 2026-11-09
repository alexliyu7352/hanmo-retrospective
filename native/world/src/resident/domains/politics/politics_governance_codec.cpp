#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 治理规则和效果计划的严格边界；目录不能伪造一个已经完成的Task事实。
#include "politics_governance.hpp"
#include "politics_governance_legacy_fields.hpp"
namespace hanmo::resident {
// 严格codec只构造值，不分配Task、Action或来源事实。
BoundaryValue encode_appeasement_rule(const AppeasementRule& value){return dynamic_economy_codec::encode(value);}
Outcome<AppeasementRule> decode_appeasement_rule(const BoundaryValue& value){try{AppeasementRule out;dynamic_economy_codec::decode(value,out,"/appeasement_rule");auto valid=validate_appeasement_rule(out);return valid.ok?Outcome<AppeasementRule>{true,std::move(out),{}}:Outcome<AppeasementRule>{false,{},valid.error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
BoundaryValue encode_governance_outcome_plan(const GovernanceOutcomePlan& value){return dynamic_economy_codec::encode(value);}
Outcome<GovernanceOutcomePlan> decode_governance_outcome_plan(const BoundaryValue& value){try{GovernanceOutcomePlan out;dynamic_economy_codec::decode(value,out,"/governance_outcome_plan");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// Task原生检查点直接嵌入本值；恢复只读，不重复加民心。
Status visit_appeasement_rule(CodecVisitor& codec,AppeasementRule& value){return dynamic_economy_codec::checkpoint(codec,"appeasement_rule",value);}
Status visit_governance_outcome_plan(CodecVisitor& codec,GovernanceOutcomePlan& value){return dynamic_economy_codec::checkpoint(codec,"governance_outcome_plan",value);}
Status emit_politics_governance_state_fields(ProjectionWriter& writer,const PoliticsGovernanceState& value){
 try{if(!value.jurisdiction_security.empty()||!value.security_consequences.empty())boundary::fail("PoliticsGovernance2Required","/politics_governance");return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_politics_governance_state(const PoliticsGovernanceState& value){return politics_projection_detail::encode_fields(value,emit_politics_governance_state_fields);}
Outcome<PoliticsGovernanceState> decode_politics_governance_state(const BoundaryValue& value){try{PoliticsGovernanceState out;dynamic_economy_codec::decode(value,out,"/politics_governance");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_politics_governance_checkpoint(CodecVisitor& codec,PoliticsGovernanceState& value){if(!value.jurisdiction_security.empty()||!value.security_consequences.empty())return codec.reject("PoliticsGovernance2Required");return dynamic_economy_codec::checkpoint(codec,"politics_governance",value);}
// 原目录字段借读出口，沿同一record供值与shape发射，不生成业务状态。
Status emit_politics_governance_catalog_fields(ProjectionWriter& writer,const PoliticsGovernanceCatalog& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_politics_governance_catalog(const PoliticsGovernanceCatalog& value){return politics_projection_detail::encode_fields(value,emit_politics_governance_catalog_fields);}
Outcome<PoliticsGovernanceCatalog> decode_politics_governance_catalog(const BoundaryValue& value){try{PoliticsGovernanceCatalog out;dynamic_economy_codec::decode(value,out,"/politics_governance");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}

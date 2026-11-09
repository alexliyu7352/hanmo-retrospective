// 严格准入与字节边界；真实占领来源另由规则核验，本叶不从坏档补造暂停或补税。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "fiscal_taxpayer_continuity_codec.hpp"
#include "../economic_payload_guard.hpp"
namespace hanmo::resident {
Outcome<FiscalTaxpayerContinuityState> make_initial_fiscal_taxpayer_continuity(const CommonStore& c,const FiscalSettlementState& fiscal){
 if(fiscal.version!=1||fiscal.initialized_tick!=c.tick()||fiscal.last_step_tick!=c.tick()||!fiscal.periods.empty()||!fiscal.assessments.empty()||!fiscal.payments.empty())return {false,{},common_failure("FiscalTaxpayerContinuityRequiresInitialWorld").error};
 FiscalTaxpayerContinuityState state;state.initialized_tick=state.last_step_tick=c.tick();return {true,std::move(state),{}};
}
Status decode_fiscal_taxpayer_continuity_profile(const BoundaryValue& value){auto unique=validate_economic_payload_fields(value);if(!unique.ok)return unique;try{using namespace boundary;fields(value,{"enabled","version"},{},fiscal_taxpayer_continuity_module);if(!get_bool(value,"enabled",fiscal_taxpayer_continuity_module)||get_int(value,"version",fiscal_taxpayer_continuity_module)!=1)return common_failure("UnsupportedFiscalTaxpayerContinuityProfile");return common_success();}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status validate_fiscal_taxpayer_continuity_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules){
 auto count=[&](const StableId& name){return std::count_if(modules.begin(),modules.end(),[&](const auto& row){return row.first==name;});};auto has=[&](const StableId& name,const StableId& version){return std::find(modules.begin(),modules.end(),std::make_pair(name,version))!=modules.end();};
 if(!enabled)return count(fiscal_taxpayer_continuity_module)?common_failure("UnexpectedFiscalTaxpayerContinuityProfile"):common_success();
 if(count(fiscal_taxpayer_continuity_module)!=1||!has(fiscal_taxpayer_continuity_module,"1"))return common_failure("FiscalTaxpayerContinuityProfileMismatch");
 for(const auto* name:{"dynamic_world","fiscal_settlement","political_takeover","economic_takeover_execution","task_takeover_execution"})if(count(name)!=1)return common_failure("FiscalTaxpayerContinuityDependenciesRequired",name);
 if(!has("dynamic_world","local_society_v1")||!has("fiscal_settlement","1")||!has("political_takeover","1")||!has("economic_takeover_execution","1")||(!has("task_takeover_execution","1")&&!has("task_takeover_execution","2")))return common_failure("FiscalTaxpayerContinuityDependenciesRequired");return common_success();
}
Status emit_fiscal_taxpayer_continuity_fields(ProjectionWriter& writer,const FiscalTaxpayerContinuityState& state){try{if(state.version!=1)boundary::fail("UnsupportedFiscalTaxpayerContinuityVersion",fiscal_taxpayer_continuity_module);return dynamic_economy_codec::emit_fields(writer,state);}catch(const boundary::DecodeFailure&failure){return writer.reject(failure.error.original.code,failure.error.original.path);}}
BoundaryValue encode_fiscal_taxpayer_continuity(const FiscalTaxpayerContinuityState& state){BoundaryValue out;BoundaryProjectionWriter writer(out);auto valid=writer.object("",[&](auto&fields){return emit_fiscal_taxpayer_continuity_fields(fields,state);});if(valid.ok)valid=writer.finish();if(!valid.ok)throw boundary::DecodeFailure{valid.error};return out;}
Outcome<FiscalTaxpayerContinuityState> decode_fiscal_taxpayer_continuity(const BoundaryValue& value){auto unique=validate_economic_payload_fields(value);if(!unique.ok)return {false,{},unique.error};try{auto state=dynamic_economy_codec::decode_root<FiscalTaxpayerContinuityState>(value,fiscal_taxpayer_continuity_module);if(state.version!=1)boundary::fail("UnsupportedFiscalTaxpayerContinuityVersion",fiscal_taxpayer_continuity_module);return {true,std::move(state),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status visit_fiscal_taxpayer_continuity_checkpoint(CodecVisitor& codec,FiscalTaxpayerContinuityState& state){if(!codec.reading()&&state.version!=1)return codec.reject("UnsupportedFiscalTaxpayerContinuityVersion",fiscal_taxpayer_continuity_module);auto result=dynamic_economy_codec::checkpoint(codec,fiscal_taxpayer_continuity_module,state);if(!result.ok)return result;return state.version==1?codec.status():codec.reject("UnsupportedFiscalTaxpayerContinuityVersion",fiscal_taxpayer_continuity_module);}
}

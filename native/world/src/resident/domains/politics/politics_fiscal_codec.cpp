#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
#include "resident/checkpoint/projection_writer.hpp"
// 财政政策保留严格历史字段；解码缺政策不补常赋，也不重新征收旧期。
#include "politics_fiscal.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "politics_fiscal_codec_fields.hpp"

namespace hanmo::resident {
// 严格JSON拒绝缺失、重复和未知字段，语义真实性再由财政验证门核验。
Outcome<PoliticalFiscalState> decode_political_fiscal_state(const BoundaryValue& value){try{PoliticalFiscalState out;dynamic_economy_codec::decode(value,out,"/political_fiscal");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 编码不修改历史，不把无政策转换成normal。
Status emit_political_fiscal_state_fields(ProjectionWriter& writer,const PoliticalFiscalState& value){
 try{if(value.version!=1)return writer.reject("PoliticalFiscalLegacyVersionRequired");for(const auto& row:value.policies.rows())if(row.second.issued_at)return writer.reject("PoliticalFiscalLegacyMomentForbidden",row.first);return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_fiscal_state(const PoliticalFiscalState& value){return politics_projection_detail::encode_fields(value,emit_political_fiscal_state_fields);}
// 原生检查点使用同一字段与字符串枚举，冷恢复不重复签发。
Status visit_political_fiscal_checkpoint(CodecVisitor& codec,PoliticalFiscalState& value){if(!codec.reading()){if(value.version!=1)return codec.reject("PoliticalFiscalLegacyVersionRequired");for(const auto& row:value.policies.rows())if(row.second.issued_at)return codec.reject("PoliticalFiscalLegacyMomentForbidden",row.first);}else value.version=1;return dynamic_economy_codec::checkpoint(codec,"political_fiscal",value);}
// 公共注册只转交严格叶codec，不复制财政解释。
// 原目录字段借读出口，沿同一record供值与shape发射，不生成业务状态。
Status emit_political_fiscal_catalog_fields(ProjectionWriter& writer,const PoliticalFiscalCatalog& value){
 try{if(value.version!=1)boundary::fail("PoliticalFiscalLegacyVersionRequired",{});return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_fiscal_catalog(const PoliticalFiscalCatalog& value){return politics_projection_detail::encode_fields(value,emit_political_fiscal_catalog_fields);}
Outcome<PoliticalFiscalCatalog> decode_political_fiscal_catalog(const BoundaryValue& value){try{PoliticalFiscalCatalog out;dynamic_economy_codec::decode(value,out,"/political_fiscal");return {true,out,{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
BoundaryValue encode_political_set_tax(const PoliticalSetTaxCommand& value){return dynamic_economy_codec::encode(value);}
Outcome<PoliticalSetTaxCommand> decode_political_set_tax(const BoundaryValue& value){try{PoliticalSetTaxCommand out;dynamic_economy_codec::decode(value,out,"/political_set_tax");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
namespace {
// 原record只访问三个标量字段；写适配不修改借入的已发布回执。
struct FiscalEffectProjectionFields {ProjectionWriter& writer;template<class T>void operator()(const char* name,T& value){writer.field(name,value);}};
}
// 同一字段路径供原JSON编码与checkpoint形状核验使用。
Status emit_political_fiscal_effects_fields(ProjectionWriter& w,const PoliticalFiscalEffects& value){FiscalEffectProjectionFields fields{w};dynamic_economy_codec::record(fields,const_cast<PoliticalFiscalEffects&>(value));return w.status();}
BoundaryValue encode_political_fiscal_effects(const PoliticalFiscalEffects& value){BoundaryValue result;BoundaryProjectionWriter writer(result);auto status=writer.object("",[&](auto& fields){return emit_political_fiscal_effects_fields(fields,value);});if(status.ok)status=writer.finish();if(!status.ok)boundary::fail(status.error.original.code,status.error.original.path);return result;}
Outcome<PoliticalFiscalEffects> decode_political_fiscal_effects(const BoundaryValue& value){try{PoliticalFiscalEffects out;dynamic_economy_codec::decode(value,out,"/political_fiscal_effects");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_political_fiscal_effects(CodecVisitor& codec,PoliticalFiscalEffects& value){return dynamic_economy_codec::checkpoint(codec,"political_fiscal_effects",value);}
}

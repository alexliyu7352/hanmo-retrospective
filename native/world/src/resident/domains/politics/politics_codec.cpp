#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 政治新域独立严格codec；旧组织和世界布局不因本叶存在而改变。
#include "politics_codec_fields.hpp"
#include "politics_model.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include <optional>

namespace hanmo::resident {
// 冷目录借外层同一候选任务；只登记原目录集合，独立解码保原边界。
Outcome<PoliticsCatalog> decode_politics_catalog(const BoundaryValue &root) {
  try {
    std::optional<ScopedCheckpointJsonDecode> scope;
    if (ScopedCheckpointBoundaryDecode::current())
      scope.emplace(CheckpointCatalogJsonKind::politics);
    PoliticsCatalog out;
    dynamic_economy_codec::decode(root, out, "/politics");
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Outcome<PoliticsState> decode_politics_state(const BoundaryValue &root) {
  try {
    PoliticsState out;
    dynamic_economy_codec::decode(root, out, "/politics");
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
// 原目录字段借读出口，沿同一record供值与shape发射，不生成业务状态。
Status emit_politics_catalog_fields(ProjectionWriter& writer,const PoliticsCatalog& c){
 try{return dynamic_economy_codec::emit_fields(writer,c);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_politics_catalog(const PoliticsCatalog& c){return politics_projection_detail::encode_fields(c,emit_politics_catalog_fields);}
Status emit_politics_state_fields(ProjectionWriter& writer,const PoliticsState& s){
 try{for(const auto& row:s.facts.rows())if(row.second.occurred_at)boundary::fail("Politics2Required","/politics/facts/"+row.first);return dynamic_economy_codec::emit_fields(writer,s);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_politics_state(const PoliticsState& s){return politics_projection_detail::encode_fields(s,emit_politics_state_fields);}
Status visit_politics_checkpoint(CodecVisitor& c,PoliticsState& s){for(const auto& row:s.facts.rows())if(row.second.occurred_at)return c.reject("Politics2Required");return dynamic_economy_codec::checkpoint(c,"politics",s);}
} // namespace hanmo::resident

// 组织检查点遍历全部持久字段；恢复不重放资源结果。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "organization_codec_detail.hpp"
#include "organization_service_effects_codec.hpp"
namespace hanmo::resident {
Status visit_organization_checkpoint(CodecVisitor& codec,OrganizationState& state,bool named_membership) {
 if(!codec.reading()){
  auto selected=validate_organization_service_effects(state,0);if(!selected.ok)return selected;
  for(const auto& row:state.facts.rows())if(row.second.occurred_at)return codec.reject("OrganizationExactFactCodecRequired",row.first);
 }
 auto valid=dynamic_economy_codec::checkpoint(codec,"organization",state,false,named_membership);
 if(valid.ok&&codec.reading())valid=validate_organization_service_effects(state,0);
 return valid;
}
}

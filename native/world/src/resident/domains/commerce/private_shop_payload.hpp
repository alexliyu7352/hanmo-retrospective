// Common事实/Action只认本域具名实体，不能凭domain跳过冷载或导出校验。
#pragma once
#include "private_shop_boundary.hpp"
namespace hanmo::resident {
inline constexpr std::uint32_t private_shop_checkpoint_feature=262144u;
bool private_shop_command_kind(const StableId&);
Status validate_private_shop_pair(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules);
Status validate_private_shop_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const PrivateShopCatalog*,const PrivateShopState*);
// Borrowed original header/key/unique-table identity, without projecting a payload tree.
Status validate_private_shop_fact_identity(const CommonStore&,const PrivateShopState&,const FactRecord&);
Status validate_private_shop_fact_header(const CommonStore&,const PrivateShopState&,const FactRecord&);
class ProjectionWriter;
Status validate_private_shop_fact_header(const CommonStore&,const PrivateShopState&,const FactRecord&,ProjectionWriter&);
Status emit_private_shop_fact_payload_fields(ProjectionWriter&,const CommonStore&,const PrivateShopState&,const FactRecord&);
Status emit_private_shop_action_payload_fields(ProjectionWriter&,const CommonStore&,const PrivateShopState&,const ActionCoreView&);
Outcome<BoundaryValue> encode_private_shop_fact_payload(const CommonStore&,const PrivateShopState&,const FactRecord&);
Outcome<BoundaryValue> encode_private_shop_action_payload(const CommonStore&,const PrivateShopState&,const ActionCoreView&);
Status validate_private_shop_action_payload(const CommonStore&,const PrivateShopState&,const ActionCoreView&,const BoundaryValue&);
}

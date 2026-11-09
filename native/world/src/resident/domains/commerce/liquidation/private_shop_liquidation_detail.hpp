// 身后授权的事实序与生命前界共用一处；不能以尚未登记死亡代替实际死因顺序。
#pragma once
#include "private_shop_liquidation.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
namespace hanmo::resident::shop_liquidation_detail {
inline bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
Outcome<Integer> fact(const CommonStore&,const StableId&,const StableId& prefix,const StableId& kind,const StableId& domain);
bool before(const CommonStore&,const StableId&,const StableId&);
// 实际原死因在边界后可保旧授权；死因已在边界前即拒，哪怕生命登记尚未发生。
Status alive_before(const PrivateShopLiquidationContext&,const StableId&,Integer,const StableId& boundary={});
const DialogueNode* declaration(const InteractionCatalog&,const StableId& dialogue,const StableId& node);
const PrivateShopExecutorRevocation* revocation(const PrivateShopLiquidationState&,const StableId& mandate);
}

// 两个零时具名命令只声明/撤回本人遗赠，不办理死后转账或代替受益人接受。
#pragma once
#include "family_bequest.hpp"
namespace hanmo::resident {
struct FamilyBequestCommand { StableId kind,beneficiary_actor_id,declaration_id;SuccessionPersonalSelection selected; };
struct FamilyBequestQuote { std::optional<FamilyBequestDeclarationQuote> declaration;std::optional<FamilyBequestRevocationQuote> revocation; };
Outcome<FamilyBequestCommand> decode_family_bequest_command(const StableId& kind,const BoundaryValue&);
Outcome<FamilyBequestQuote> preview_family_bequest_command(const CommonStore&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const FamilyBequestPolicy&,const FamilyBequestState&,const FamilyBequestPlayerRead&,const StableId& actor,const FamilyBequestCommand&,const FamilyLifeHistoryRead4* life4=nullptr,const FamilyContinuationPeopleRead4* people=nullptr);
Outcome<FamilyEffect> apply_family_bequest_command(CommonStore&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const FamilyBequestPolicy&,FamilyBequestState&,const FamilyBequestPlayerRead&,const StableId& actor,const FamilyBequestCommand&,const FamilyBequestQuote&,const FamilyLifeHistoryRead4* life4=nullptr,const FamilyContinuationPeopleRead4* people=nullptr);
// 有限本人页展示当前原收养候选与直接可用私产；不扫描全世界遗产或军事事实。
Outcome<BoundaryValue> query_family_bequest_page(const CommonStore&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const FamilyBequestPolicy&,const FamilyBequestState&,const FamilyBequestPlayerRead&,const CommonCatalog&,Integer page,const FamilyLifeHistoryRead4* life4=nullptr,const FamilyContinuationPeopleRead4* people=nullptr);
// 原FamilyEffect tag复用但严核本域事实，坏缓存不能变成另一人的遗赠或交付事实。
Status validate_family_bequest_receipt(const FamilyBequestState&,const StableId& kind,const StableId& actor,Integer tick,const FamilyEffect&);
}

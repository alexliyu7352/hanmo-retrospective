// 成年收养只在原Family唯一事实账中添加关系；不生成演员或变更任何钱货权属。
#pragma once
#include "family_model.hpp"
namespace hanmo::resident {
Status validate_adult_adoptions(const CommonStore&,const FamilyState&,const CommonCatalog&,
    const InteractionCatalog&,const AdultAdoptionRequestCatalog*);
Outcome<AdultAdoptionRequestQuote> preview_adult_adoption(const CommonStore&,const FamilyState&,
    const InteractionState&,const InteractionCatalog&,const InteractionAccess&,
    const AdultAdoptionRequestCatalog&,const StableId& adopter,const StableId& adoptee,const StableId& request_id);
// 调用方仍须使用共同事务的本人/幂等准入；本函数重验报价并保ID与Family头原子一致。
Outcome<StableId> accept_adult_adoption(CommonStore&,FamilyState&,const InteractionState&,
    const InteractionCatalog&,const InteractionAccess&,const AdultAdoptionRequestCatalog&,
    const AdultAdoptionRequestQuote&);
// 只显示当前本人已到达的请求以及本人参与的永久关系，不枚举其他人的私人请求。
Outcome<BoundaryValue> query_adult_adoption_page(const CommonStore&,const FamilyState&,
    const InteractionState&,const InteractionCatalog&,const InteractionAccess&,
    const AdultAdoptionRequestCatalog&,Integer page=0);
}

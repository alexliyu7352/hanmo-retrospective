// 本人师承有限页仅展示亲自听取/答复与共同接受的关系，未知退出消息不从全账补给本人。
#pragma once
#include "apprenticeship_commands.hpp"
#include "apprenticeship_courses.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_apprenticeship_page(const CommonStore&,const CommonCatalog&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ApprenticeshipCatalog&,const FamilyApprenticeshipState&,const ApprenticeshipIdentityRead&,Integer page);
Outcome<BoundaryValue> query_apprenticeship_courses_page(const CommonStore&,const GrowthState&,const FamilyApprenticeshipState&,const StableId& bond_id,Integer page);
}

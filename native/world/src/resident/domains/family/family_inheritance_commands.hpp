// 公开、本人接受与分次交割各走独立具名命令，参数不能提供死亡真相或自造产权来源。
#pragma once
#include "family_inheritance.hpp"
namespace hanmo::resident {
struct FamilyInheritanceCommand { StableId kind,declaration_id,disclosure_fact_id,notice_binding_fact_id,acceptance_fact_id;InheritancePersonalSelection selected;std::optional<ActorKnownDeathReceiptRef> known_death; };
struct FamilyInheritanceQuote { std::optional<BequestDisclosureQuote> disclosure;std::optional<FamilyInheritanceAcceptanceQuote> acceptance;std::optional<InheritancePersonalQuote> settlement; };
// 同候选最小借口仅在本次调用有效，历史权源不得捕获外层旧World账本。
struct FamilyInheritanceAccess { FamilyBequestPlayerRead player;FamilyReceivedDeathNoticeRead received_notice;InheritancePrivateClearanceRead clearance;const FamilyInheritanceSources2* sources=nullptr; };
Outcome<FamilyInheritanceCommand> decode_family_inheritance_command(const StableId&,const BoundaryValue&);
Outcome<FamilyInheritanceCommand> decode_family_inheritance_command_v2(const StableId&,const BoundaryValue&);
Outcome<FamilyInheritanceQuote> preview_family_inheritance_command(const CommonStore&,const FamilyBequestState&,const FamilyDeathState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const FamilyInheritancePolicy&,const FamilyInheritanceState&,const FamilyInheritanceAccess&,const StableId& actor,const FamilyInheritanceCommand&);
Outcome<FamilyEffect> apply_family_inheritance_command(CommonStore&,const FamilyBequestState&,const FamilyDeathState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const FamilyInheritancePolicy&,FamilyInheritanceState&,const FamilyInheritanceAccess&,const StableId& actor,const FamilyInheritanceCommand&,const FamilyInheritanceQuote&);
Status validate_family_inheritance_receipt(const FamilyBequestState&,const FamilyInheritanceState&,const StableId& kind,const StableId& actor,Integer tick,const FamilyEffect&);
// 每个列表至多16项，资产只投影本人已经获知的原上限，不观察远处死者的现有钱包或包裹。
Outcome<BoundaryValue> query_family_inheritance_page(const CommonStore&,const CommonCatalog&,const FamilyBequestState&,const FamilyDeathState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const FamilyInheritancePolicy&,const FamilyInheritanceState&,const FamilyInheritanceAccess&,Integer page,Integer lot_page,const StableId& notice_binding_fact_id,const ActorKnownDeathReceiptRef* known_death=nullptr);
}

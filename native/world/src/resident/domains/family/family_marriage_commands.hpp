// 玩家婚议入口只接收具名选择，不收任意人物状态、配偶或关系数值setter。
#pragma once
#include "family_marriage.hpp"
namespace hanmo::resident {
struct FamilyMarriageCommand { StableId kind,statement_id,proposal_id,decision,marriage_id; };
struct FamilyMarriageQuote { WorldIdentity identity;FamilyMarriageCommand command;std::optional<MarriageStatementQuote> source; };
Outcome<FamilyMarriageCommand> decode_family_marriage_command(const StableId&,const BoundaryValue&);
Outcome<FamilyMarriageQuote> preview_family_marriage_command(const CommonStore&,const FamilyState&,const FamilyDeathState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const MarriageSourceCatalog&,const FamilyMarriageState&,const MarriageAccess&,const StableId& actor,const FamilyMarriageCommand&);
Outcome<FamilyEffect> apply_family_marriage_command(CommonStore&,const FamilyState&,const FamilyDeathState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const MarriageSourceCatalog&,FamilyMarriageState&,const MarriageAccess&,const StableId& actor,const FamilyMarriageCommand&,const FamilyMarriageQuote&);
BoundaryValue encode_family_marriage_quote(const FamilyMarriageQuote&);
Status validate_family_marriage_receipt(const FamilyMarriageState&,const StableId& kind,const StableId& actor,Integer tick,const FamilyEffect&);
Outcome<BoundaryValue> query_family_marriage_page(const CommonStore&,const CommonCatalog&,const FamilyState&,const FamilyDeathState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const MarriageSourceCatalog&,const FamilyMarriageState&,const MarriageAccess&,Integer page);
}

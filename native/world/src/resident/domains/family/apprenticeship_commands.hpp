// 正常师承命令只转发本人零时决定；不存在任意关系 setter 或代替课程的经验命令。
#pragma once
#include "family_apprenticeship.hpp"
#include "resident/domains/family/family_model.hpp"
namespace hanmo::resident {
struct ApprenticeshipCommand {StableId kind,statement_id,proposal_id,decision,bond_id;};
struct ApprenticeshipQuote {WorldIdentity identity;ApprenticeshipCommand command;std::optional<ApprenticeshipStatementQuote> source;};
Outcome<ApprenticeshipCommand> decode_apprenticeship_command(const StableId&,const BoundaryValue&);
BoundaryValue encode_apprenticeship_command(const ApprenticeshipCommand&);
Outcome<ApprenticeshipQuote> preview_apprenticeship_command(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ApprenticeshipCatalog&,const FamilyApprenticeshipState&,const ApprenticeshipIdentityRead&,const StableId& actor,const ApprenticeshipCommand&);
// 原 World/NPC 完整候选持 Common＋本账保存点；本写口不能独立发布或提交调用者保存点。
Outcome<FamilyEffect> apply_apprenticeship_command_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ApprenticeshipCatalog&,FamilyApprenticeshipState&,const ApprenticeshipIdentityRead&,const StableId& actor,const ApprenticeshipCommand&,const ApprenticeshipQuote&);
BoundaryValue encode_apprenticeship_quote(const ApprenticeshipQuote&);
}

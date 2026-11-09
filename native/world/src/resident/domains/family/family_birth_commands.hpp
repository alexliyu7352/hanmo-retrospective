// 本人独立决定与真实照护Action的命令外壳，不接受直接设置亲代、孩子、粮数或禀赋。
#pragma once
#include "family_child_care.hpp"
namespace hanmo::resident {
struct FamilyBirthCommand { StableId kind,arrangement_id,proposal_id,decision,accepted_id,action_id; };
struct FamilyBirthQuote { WorldIdentity identity;FamilyBirthCommand command;std::optional<BirthArrangementQuote> source;std::optional<ChildCareQuote> care; };
struct FamilyBirthCommandResult { FamilyEffect effect;StableId release_action_id; };
Outcome<FamilyBirthCommand> decode_family_birth_command(const StableId&,const BoundaryValue&);
Outcome<FamilyBirthQuote> preview_family_birth_command(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const FamilyCatalog&,const BirthArrangementCatalog&,const FamilyBirthState&,const BirthArrangementAccess&,const StableId& actor,const FamilyBirthCommand&);
// 零时本人命令在外层候选执行。family_child_care必须走原Action创建/计时入口，不能在这里即时喂养。
// 取消返回确切原Action供World释放；调用方须回滚Common＋FamilyBirth全部写集及占用。
Outcome<FamilyBirthCommandResult> apply_family_birth_command_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const StableId& actor,const FamilyBirthCommand&,const FamilyBirthQuote&);
// Action创建之前复核原命令和新鲜报价；开工后使用quote.care冻结原ChildCareStart。
Status revalidate_family_birth_quote(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const FamilyCatalog&,const BirthArrangementCatalog&,const FamilyBirthState&,const BirthArrangementAccess&,const StableId& actor,const FamilyBirthCommand&,const FamilyBirthQuote&);
BoundaryValue encode_family_birth_quote(const FamilyBirthQuote&);
Status validate_family_birth_command_receipt(const FamilyBirthState&,const StableId& kind,const StableId& actor,Integer tick,const FamilyEffect&);
Outcome<BoundaryValue> query_family_birth_page(const CommonStore&,const CommonCatalog&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const FamilyCatalog&,const BirthArrangementCatalog&,const FamilyBirthState&,const BirthArrangementAccess&,Integer page);
}

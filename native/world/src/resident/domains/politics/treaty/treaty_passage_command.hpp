// 新业务命令与旧外交variant分开，中央只登记两种明确命令及复用回执载荷。
#pragma once
#include "treaty_passage_profile.hpp"
namespace hanmo::resident {
using TreatyPassageCommand=std::variant<TreatyPassagePresentCommand,TreatyPassageSignCommand>;
struct TreatyPassageQuote {WorldIdentity identity;StableId actor_id;TreatyPassageCommand command;Integer duration=0;};
StableId treaty_passage_command_kind(const TreatyPassageCommand&);
BoundaryValue encode_treaty_passage_command(const TreatyPassageCommand&);
Outcome<TreatyPassageCommand> decode_treaty_passage_command(const StableId&,const BoundaryValue&);
BoundaryValue encode_treaty_passage_quote(const TreatyPassageQuote&);
Outcome<TreatyPassageQuote> preview_treaty_passage_command(const TreatyPassageContext&,const StableId&,const TreatyPassageCommand&);
Outcome<StableId> apply_treaty_passage_command_in_transaction(CommonStore&,TreatyPassageState&,const TreatyPassageContext&,const StableId&,const TreatyPassageCommand&,const TreatyPassageQuote&);
// 缓存回执须对应本次真实具名写入；NPC无玩家回执时仍只由本域事实证明。
Status validate_treaty_passage_receipt_sources(const TreatyPassageContext&,const std::function<bool(const CommitResult&)>& is_done);
// 只展示本人真实获得的文书；返回普通现有业务页动作，不直接执行或修改原外交状态。
Outcome<BoundaryValue> query_treaty_passage_page(const TreatyPassageContext&,const CommonCatalog&,const StableId& observer,Integer cursor=0);
Outcome<BoundaryValue> merge_treaty_passage_page(const TreatyPassageContext&,const CommonCatalog&,const StableId& observer,BoundaryValue original_page);
// 仅作者列明的NPC公职可提出本人明确签署，仍须按同一command提交并在执行时重验。
Outcome<std::vector<TreatyPassageSignCommand>> propose_treaty_passage_npc(const TreatyPassageContext&,const StableId& actor);
}

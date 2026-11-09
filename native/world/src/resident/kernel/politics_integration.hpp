// 政治接线显式借各权威域；不依赖尚未登记的WorldBundle字段或通用JSON业务总线。
#pragma once
#include "resident/domains/politics/politics_views.hpp"
#include "resident/domains/politics/politics_view_index.hpp"
#include "resident/domains/politics/politics_organization_runtime.hpp"
#include <functional>
namespace hanmo::resident {
struct WorldBundle;
// 读取只拿本业务实际需要的同候选对象，UI不取得可变世界。
struct PoliticsReadContext {
    const CommonStore& common; const CommonCatalog& common_catalog;
    const PoliticsState& politics; const PoliticsCatalog& politics_catalog;
    const OrganizationState& organization; const OrganizationCatalog& organization_catalog;
    const LocalEconomyState& economy; const SocialState& social; const ActivityRead& activity;
    const PoliticalNavigationRead* navigation=nullptr;
    const PoliticsViewIndex* view_index=nullptr;
    PoliticalDismissalPreviewRead dismissal_preview;
    // 公开入口只借原世界获取真实拥有体，不允许外部注入 runtime 或过期读票。
    const WorldBundle* original_world=nullptr;
};
// 只有同一事务中的共同事实、政治状态及唯一组织Office允许本域变更。
struct PoliticsWriteContext {
    CommonStore& common; const CommonCatalog& common_catalog;
    PoliticsState& politics; const PoliticsCatalog& politics_catalog;
    OrganizationState& organization; const OrganizationCatalog& organization_catalog;
    const LocalEconomyState& economy; const SocialState& social; const ActivityRead& activity;
    const PoliticalNavigationRead* navigation=nullptr;
    const PoliticsViewIndex* view_index=nullptr;
    PoliticalDismissalPreviewRead dismissal_preview;
    std::function<Outcome<PoliticsEffects>(const StableId&,const PoliticalDismissCommand&)> dismissal_apply;
    // 写入口由同一候选获取并保活 writer，预检只借该 writer 的只读视图。
    WorldBundle* candidate_world=nullptr;
    PoliticsReadContext read() const;
};
// 中央唯一union只需登记类型及这个getter，本叶负责缓存回执的真实来源校验。
using PoliticsReceiptReader=std::function<const PoliticsEffects*(const CommitResult&)>;
bool is_politics_command_kind(const StableId&);
Outcome<PoliticsQuote> preview_political_business(const PoliticsReadContext&,const StableId& actor,const PoliticsCommand&);
// apply只供中央完整候选或NPC子事务使用；失败由调用方回滚Common和领域写集。
Outcome<PoliticsEffects> apply_political_business(PoliticsWriteContext&,const StableId& actor,const PoliticsCommand&,const PoliticsQuote&);
Outcome<BoundaryValue> query_political_business(const PoliticsReadContext&,const StableId& observer,const StableId& place);
Status bootstrap_political_business(PoliticsWriteContext&);
Outcome<PoliticsEffects> finish_political_business(PoliticsWriteContext&);
// 完整政治正文已由同候选精确owner核验后，仍独立复核全部原命令缓存及分配历史。
Status validate_political_business_receipts(const PoliticsReadContext&,bool capability_enabled,const PoliticsReceiptReader&);
Status validate_political_business(const PoliticsReadContext&,bool capability_enabled,const PoliticsReceiptReader&);
Status validate_political_business(const PoliticsReadContext&,bool,const PoliticsReceiptReader&,const PoliticalTerritoryRead*);
Status validate_politics_receipt_source(const CommonStore&,const PoliticsState&,const OrganizationState&,const StableId& actor,const StableId& kind,Integer tick,const PoliticsEffects&);
BoundaryValue encode_politics_quote(const PoliticsQuote&);
Outcome<PoliticsQuote> decode_politics_quote(const BoundaryValue&);
BoundaryValue encode_politics_effects(const PoliticsEffects&);
Outcome<PoliticsEffects> decode_politics_effects(const BoundaryValue&);
Status visit_politics_effects(CodecVisitor&,PoliticsEffects&);
}

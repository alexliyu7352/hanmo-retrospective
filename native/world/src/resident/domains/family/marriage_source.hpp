// 婚议只读来源：明确原处境和本人节点意愿，不在来源层创设配偶、迁居或财产许可。
#pragma once
#include "resident/domains/interaction/dialogue_node_source.hpp"
namespace hanmo::resident {
// 缺条目或unknown均为未知；已婚原关系必须由双方相互一致的作者条目证明。
struct MarriageOrigin {
    StableId actor_id,status;
    std::optional<StableId> spouse_actor_id;
};
// offer_marriage表达发言人向指定本人提出个人婚议；accept_dissolution只表达愿意解除。
// 两种都不能自行办理，最终账还须匹配唯一原提议/婚姻和另一方当前独立决定。
struct MarriageStatementDefinition {
    StableId id,speaker_actor_id,listener_actor_id,dialogue_id,node_id,intent;
    Integer terms_version=1;
};
struct MarriageSourceCatalog {
    bool enabled=false;
    OrderedTable<MarriageOrigin> origins;
    OrderedTable<MarriageStatementDefinition> statements;
};
// 永久证明存原World、生日、节点和当时会面；不把恢复前execution_epoch作永久许可。
struct MarriageStatementSource {
    StableId statement_id,speaker_actor_id,listener_actor_id,dialogue_id,node_id,node_kind;
    StableId intent,session_id,scenario_id;
    WorldId world_id{};
    InteractionContext context;
    Integer terms_version=1,tick=0,revision=0,speaker_birth_tick=0,listener_birth_tick=0;
    FieldOrder field_order;
};
struct MarriageStatementQuote { AuthoredNodeSourceProof source;MarriageStatementSource statement; };
class CodecVisitor;
struct BoundaryValue;
// 作者原处境必须明确且配偶相互指向；这不是本人当前婚姻或血亲不存在的证明。
Status validate_marriage_source_catalog(const MarriageSourceCatalog&,const CommonCatalog&,const InteractionCatalog&);
bool marriage_statement_equal(const MarriageStatementSource&,const MarriageStatementSource&);
// 当前玩家是听取提议/答复的另一当事人；原节点发言人的意愿不会由玩家点击代签。
Outcome<MarriageStatementQuote> preview_marriage_statement(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const MarriageSourceCatalog&,const StableId& listener,const StableId& statement_id);
Status revalidate_marriage_statement(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const MarriageSourceCatalog&,const MarriageStatementQuote&);
// 冷来源不要求旧会话仍开着或双方今天仍存活；死亡前后及原关系顺序由婚姻账另核。
Status validate_marriage_statement_source(const CommonState&,const CommonCatalog&,const InteractionCatalog&,const MarriageSourceCatalog&,const MarriageStatementSource&);
Outcome<MarriageSourceCatalog> decode_marriage_source_catalog(const BoundaryValue&);
Outcome<MarriageStatementSource> decode_marriage_statement_source(const BoundaryValue&);
BoundaryValue encode_marriage_statement_source(const MarriageStatementSource&);
Status visit_marriage_statement_source(CodecVisitor&,MarriageStatementSource&);
}

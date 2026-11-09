// 原身体两类来源的唯一 SourceRef 技术映射；不产生 Common Fact 或第二身体账。
#pragma once
#include "character_condition_read.hpp"
namespace hanmo::resident {
struct CharacterConditionSourceView {
 SourceRef source;Integer source_schema=0;StableId actor_id;WorldMoment occurred_at;
 Integer condition_revision=0;
 const InitialConditionSeed* initial_seed=nullptr;
 const ConditionReceiptV2* receipt=nullptr;
};
SourceRef initial_condition_seed_source(const InitialConditionSeedRef&);
SourceRef character_condition_receipt_source(const ConditionReceiptRefV2&);
// Seed /1 = actor_start_context/2; seed /2 = actor_start_context/3. Receipt remains /2.
Outcome<Integer> read_initial_condition_seed_schema(const ActorFoundationCatalog&);
// Selection is derived only from the admitted catalog, never variant contents.
Outcome<CharacterConditionSourceView> read_character_condition_source(const CharacterConditionSources&,const SourceRef&,Integer source_schema);
Outcome<CharacterConditionSourceView> read_character_condition_head_source(const CharacterConditionSources&,const ActorConditionHead&,const WorldMoment& actual_boundary);
}
namespace hanmo::resident {
// Resolve an original seed/receipt frozen by the actual accepting consumer.
// This does not invent a causal link between an arbitrary Common fact and a head.
Outcome<CharacterConditionAtHead> read_character_condition_at_source(const CharacterConditionSources&,const SourceRef&,Integer source_schema,const WorldMoment& actual_boundary);
}

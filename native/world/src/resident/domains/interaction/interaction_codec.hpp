// 交互冷边界只保类型化状态，未知动态字段拒绝而非落入extras。
#pragma once
#include "interaction_model.hpp"
#include <string_view>
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
class ScopedCheckpointJsonDecode;
namespace interaction_codec {
InteractionContext decode_context(const BoundaryValue&,const StableId&);
// Same original schema/parser; the actual decoded outer owner keeps all charges.
// The caller supplies its exact same-job budget and original diagnostic path.
Outcome<InteractionContext> decode_context(const BoundaryValue&,ScopedCheckpointJsonDecode&,std::string_view path);
BoundaryValue encode_context(const InteractionContext&);
// 有限视图冷封装复用同一字段/原序编码；失败沿既有DecodeFailure边界交调用者处理。
BoundaryValue encode_command(const DialogueCommand&);
BoundaryValue encode_outcome(const DialogueOutcome&);
Status visit_context(CodecVisitor&,InteractionContext&);
}
Outcome<InteractionCatalog> decode_interaction_catalog(const BoundaryValue& data,std::shared_ptr<const CommonCatalog> common);
Outcome<InteractionState> decode_interaction_seed(const BoundaryValue& state);
Outcome<InteractionState> decode_interaction_selected_seed(const BoundaryValue&,Integer selected_physical_version);
Outcome<BoundaryValue> export_interaction_selected_state(const InteractionState&,Integer selected_physical_version);
Outcome<BoundaryValue> export_interaction_state(const InteractionState&);
Outcome<InteractionCommand> decode_interaction_command(const StableId& kind,const BoundaryValue& params);
}

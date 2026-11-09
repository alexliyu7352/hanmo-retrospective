// 本段仅共用严格序列化，不重建人物或关系，不把解码成功解释为来源通过。
#pragma once
#include "known_personal_dialogue_contract.hpp"
#include "resident/domains/tasks/task_dynamic_codec.hpp"
#include <set>
namespace hanmo::resident::known_personal_codec {
template<class A>Status origin(A& a,KnownPersonalActorOrigin& o){a.field("kind",o.kind);a.field("actor_id",o.actor_id);a.field("birth_fact_id",o.birth_fact_id);a.field("creation_fact_id",o.creation_fact_id);return a.field("prototype_id",o.prototype_id);}
inline Status unique(const BoundaryValue& v,std::size_t depth=0){if(depth>CodecVisitor::max_depth)return common_failure("KnownPersonalDialogueTooDeep");if(const auto* d=v.as<BoundaryValue::Dict>()){if(d->size()>CodecVisitor::max_items)return common_failure("KnownPersonalDialogueTooLarge");std::set<StableId> keys;for(const auto& row:*d){if(!keys.insert(row.first).second)return common_failure("DuplicateKnownPersonalDialogueField",row.first);auto r=unique(row.second,depth+1);if(!r.ok)return r;}}if(const auto* a=v.as<BoundaryValue::Array>()){if(a->size()>CodecVisitor::max_items)return common_failure("KnownPersonalDialogueTooLarge");for(const auto& row:*a){auto r=unique(row,depth+1);if(!r.ok)return r;}}return common_success();}
}

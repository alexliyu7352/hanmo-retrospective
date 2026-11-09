// ActorConditionHead 的唯一字段序；嵌入消费者借同 callback，不造第二 schema。
#pragma once
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident::actor_condition_codec {
template<class C>Status head(C& c,ActorConditionHead& h){c.field("actor_id",h.actor_id);c.field("condition_revision",h.condition_revision);c.object("initial_condition_seed_ref",[&](auto& v){return v.field("seed_key",h.initial_condition_seed_ref.seed_key);});return c.optional("latest_receipt_ref",h.latest_receipt_ref,[](auto& v,auto& ref){return v.field("result_id",ref.result_id);});}
}

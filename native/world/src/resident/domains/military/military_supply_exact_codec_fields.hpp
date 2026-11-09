#pragma once
#include "military_exact_sources.hpp"
#include "resident/domains/travel/movement_record_codec_fields.hpp"
namespace hanmo::resident::military_supply_exact_fields {
// Preserve the existing source2 seven fields and their original nested scalar
// shape. This is the original dynamic_military slot, not another supply ledger.
template<class C>Status value(C& c,MilitarySupplyEffectSource2& v){
 using movement_record_fields::field;const MovementRecordCodecSelection original{};
 field(c,"original_supply_fact_ref",v.original_supply_fact_ref,original);
 field(c,"period_ref",v.period_ref,original);
 field(c,"opening_source_ref",v.opening_source_ref,original);
 field(c,"occurred_at",v.occurred_at,original);
 field(c,"selected_named_actor_ids",v.selected_named_actor_ids,original);
 field(c,"shortage_value",v.shortage_value,original);
 field(c,"original_rule_ref",v.original_rule_ref,original);return c.status();
}
}

// The actual original CampaignDepart acceptance is retained inside the S1
// opening which it starts. It is not a second campaign or a spendable budget.
#pragma once
#include "military_authority_contract.hpp"
#include "military_field_command_contract.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident {
enum class MilitaryMarchOrderKind { campaign_depart,finite_return };
struct MilitaryGroupOrderAcceptance {
    MilitaryMarchOrderKind kind=MilitaryMarchOrderKind::campaign_depart;
    StableId campaign_id,army_id,issuing_actor_id,commander_actor_id;
    SourceRef formation_source;
    MilitaryFieldCommandSource command_source;
    MilitaryAuthorityRef issuing_authority;
    ActorConditionHead issuing_condition_head,commander_condition_head;
    StableId destination_settlement_id;
    std::optional<StableId> return_garrison_id;
    WorldMoment accepted_at;
    std::vector<SourceRef> original_authority_sources;
    // A finite return follows its real terminal/withdrawal/expiry source.
    // Depart has no fabricated earlier order fact: this actual opening is it.
    std::optional<SourceRef> original_return_cause;
};
}

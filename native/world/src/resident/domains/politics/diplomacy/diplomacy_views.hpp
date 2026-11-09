// 外交页只展示实际见证过的提议版本；派生索引不进入银行或替代政治账。
#pragma once
#include "diplomacy_replies.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <map>
namespace hanmo::resident {
struct DiplomaticKnownOffer {
  StableId offer_id, fact_id;
};
struct DiplomaticProposalChoice {
  ActingAuthority authority;
  StableId policy_id, terms_id;
};
struct DiplomacyPageRead {
  const CommonStore &common;
  const CommonCatalog &common_catalog;
  const OrganizationState &organizations;
  const OrganizationCatalog &organization_catalog;
  const DiplomacyCatalog &catalog;
  const DiplomacyState &state;
  const ActivityRead &activity;
};
struct DiplomacyViewIndex {
  WorldIdentity identity;
  std::size_t fact_count = 0, offer_count = 0, mission_count = 0;
  bool initialized = false;
  CheckpointMap<StableId, std::vector<DiplomaticKnownOffer>> by_observer;
  CheckpointMap<StableId, std::vector<DiplomaticProposalChoice>>
      proposal_choices;
};
struct DiplomaticOfferView {
  StableId offer_id, source_fact_id, policy_id, policy_name_key, terms_id,
      terms_name_key;
  StableId issuer_organization_id, target_organization_id, envoy_actor_id,
      known_status;
  StableId mission_authority_fact_id;
  Integer terms_revision = 0, offer_revision = 0, issued_tick = 0,
          expires_tick = 0, known_tick = 0;
};
struct DiplomaticProposalView {
  DiplomaticProposalChoice choice;
  StableId policy_name_key, terms_name_key, disabled_code;
  bool enabled = false;
};
struct DiplomacyPageView {
  WorldIdentity source_identity;
  StableId observer_actor_id, place_id;
  std::vector<DiplomaticOfferView> offers;
  std::vector<DiplomaticProposalView> proposal_options;
  bool truncated = false;
};
// 只在同一候选最终identity发布前重建；读页不能扫全世界寻找未知人或未知提议。
void rebuild_diplomacy_view_index(const DiplomacyPageRead &,
                                  DiplomacyViewIndex &);
Outcome<DiplomacyPageView>
build_diplomacy_page_view(const DiplomacyPageRead &, const DiplomacyViewIndex &,
                          const StableId &observer_actor_id,
                          const StableId &current_place_id);
} // namespace hanmo::resident

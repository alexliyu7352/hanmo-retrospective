// 实际当面委任只产生政治批准与本人知情；接受与劳动仍由任务域的本人命令承担。
#pragma once
#include "diplomacy_missions.hpp"
#include "diplomacy_v2_state.hpp"
#include "diplomacy_v2_catalog.hpp"
namespace hanmo::resident {
inline constexpr const char* diplomatic_delegate_command_kind = "diplomatic_delegate_envoy";
inline constexpr const char* diplomatic_mission_known_fact_kind = "diplomatic_mission_communicated";
struct DiplomaticDelegateCommand { StableId delegation_policy_id; Integer expected_world_revision = 0; };
struct DiplomaticDelegateQuote {
    WorldIdentity source_identity;
    DiplomaticOffer proposal;
    DiplomaticMissionApproval mission;
    FaceToFaceContactProof contact;
};
Status validate_diplomacy_v2_catalog(const CommonCatalog&, const OrganizationCatalog&, const DiplomacyCatalog&, const DiplomacyV2Catalog&);
Outcome<DiplomacyV2Catalog> decode_diplomacy_v2_catalog(const BoundaryValue&);
BoundaryValue encode_diplomacy_v2_catalog(const DiplomacyV2Catalog&);
Outcome<DiplomaticDelegateQuote> preview_diplomatic_delegation(const DiplomaticPresentRead&, const CommonCatalog&,
    const ActivityRead&, const DiplomacyV2Catalog&, const DiplomacyV2State&, const StableId& issuer_actor,
    const DiplomaticDelegateCommand&);
Outcome<StableId> delegate_diplomatic_mission_in_transaction(CommonStore&, DiplomacyState&, DiplomacyV2State&,
    const DiplomaticPresentRead&, const CommonCatalog&, const ActivityRead&, const DiplomacyV2Catalog&,
    const StableId& issuer_actor, const DiplomaticDelegateCommand&);
// 冷读核永久接触来源，不要求当年的会话仍打开或签发人仍在任。
Status validate_diplomatic_mission_knowledge(const DiplomaticHistoryRead&, const DiplomacyV2Catalog&, const DiplomacyV2State&);
class DiplomaticDelegatedMissionAccess final : public DiplomaticMissionRead, public DiplomacyPresentationRead {
public:
    DiplomaticDelegatedMissionAccess(const DiplomaticPresentRead& read, const DiplomacyV2Catalog& catalog, const DiplomacyV2State& state)
        : read_{read.common,read.organizations,read.organization_catalog,read.catalog,read.state}, catalog_(catalog), state_(state), original_(read.common, read.organizations, read.organization_catalog, read.catalog, read.state) {}
    DiplomaticDelegatedMissionAccess(const DiplomaticHistoryRead& read, const DiplomacyV2Catalog& catalog, const DiplomacyV2State& state)
        : read_(read), catalog_(catalog), state_(state), original_(read.common,read.organizations,read.organization_catalog,read.catalog,read.state) {}
    Outcome<DiplomaticMissionApproval> diplomatic_mission_for_acceptance(const StableId&, const StableId&) const override;
    Outcome<DiplomaticMissionApproval> diplomatic_mission_source(const StableId& id) const override { return original_.diplomatic_mission_source(id); }
    Outcome<DiplomaticMissionContinuity> diplomatic_mission_continuity(const StableId& id,const StableId& actor) const override { return original_.diplomatic_mission_continuity(id,actor); }
    Outcome<DiplomaticPresentationSource> diplomatic_presentation_source(const StableId& id) const override { return original_.diplomatic_presentation_source(id); }
private:
    DiplomaticHistoryRead read_; const DiplomacyV2Catalog& catalog_; const DiplomacyV2State& state_; DiplomaticMissionAccess original_;
};
}

// OC1's canonical bounded references. Original political/organization owners retain the terms.
#pragma once
#include "resident/model/commitment_ref.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
enum class MessageChannelKind {face_to_face,custodian_pickup,carrier_delivery,scheduled_role_report};
enum class RoleDeliveryPolicy {current_authorized_holder,institutional_custody};
struct ActorAddress {StableId actor_id;};
struct CustodianActor {StableId actor_id;};
struct DeliverableProp {StableId prop_id;};
using CommunicationCustodian=std::variant<CustodianActor,DeliverableProp>;
struct CustodyAddress {StableId receiver_actor_id,place_id;CommunicationCustodian custodian;};
struct RoleAddress {StableId organization_id,office_id;ScopeRef scope;StableId site_id;RoleDeliveryPolicy delivery_policy=RoleDeliveryPolicy::current_authorized_holder;StableId required_service_kind;};
using RegisteredAddress=std::variant<ActorAddress,CustodyAddress,RoleAddress>;
enum class OfficialContentKind {appointment_offer,appointment_withdrawal,office_order,governance_offer,treaty_offer,treaty_counteroffer,treaty_ratification,strategic_order,takeover_notice,succession_notice,amnesty_offer,naval_incorporation_offer,sect_duty_offer,sect_service_offer,rescue_release_offer,martial_compact_offer,raid_participation_offer,crew_agreement_offer,naval_compact_offer,treaty_release_request=19,treaty_aid_resource_proposal=20};
struct OfficialContentRef {OfficialContentKind kind=OfficialContentKind::appointment_offer;StableId id;TermsRevisionRef terms_revision;};
struct ActorReceiver {StableId actor_id;std::optional<StableId> office_id;std::optional<Integer> office_epoch;};
struct CustodyReceiver {StableId organization_id,office_id,place_id;CommunicationCustodian custodian;};
struct DeliverySourceView {WorldIdentity source_identity;StableId delivery_fact_id,message_id;OfficialContentRef content_ref;RegisteredAddress address;MessageChannelKind channel=MessageChannelKind::face_to_face;Integer created_tick=0,delivered_tick=0;std::variant<ActorReceiver,CustodyReceiver> receiver;std::optional<SourceRef> authorization_source_ref;WorldMoment created_at,delivered_at;};
enum class OfficialDossierAccessKind {read_authorized_excerpt,take_document_custody};
struct OfficialDossierAccessReceipt {StableId access_fact_id,delivery_fact_id,actor_id,organization_id,office_id;Integer office_epoch=0;ScopeRef scope;OfficialDossierAccessKind kind=OfficialDossierAccessKind::read_authorized_excerpt;Integer completed_tick=0;SourceRef authorization_source_ref;WorldMoment completed_at;};
Status validate_registered_communication_address(const RegisteredAddress&);
Status validate_official_content_ref(const OfficialContentRef&);
bool same_registered_communication_address(const RegisteredAddress&,const RegisteredAddress&);
bool same_communication_custodian(const CommunicationCustodian&,const CommunicationCustodian&);
const char* message_channel_name(MessageChannelKind)noexcept;
Outcome<MessageChannelKind> read_message_channel_kind(const StableId&);
}

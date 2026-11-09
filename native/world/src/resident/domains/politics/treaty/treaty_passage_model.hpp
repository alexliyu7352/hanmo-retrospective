// 双签是外交回复之后独立的明示认可；旧diplomacy段与原条款字节保持不变。
#pragma once
#include "treaty_passage_contract.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_present.hpp"
#include "resident/domains/politics/politics_model.hpp"
namespace hanmo::resident {
inline constexpr const char* treaty_passage_domain="politics";
inline constexpr const char* treaty_passage_delivery_kind="treaty_passage_document_presented";
inline constexpr const char* treaty_passage_signature_kind="treaty_passage_signed";
inline constexpr const char* treaty_passage_ratify_command="diplomatic_ratify_passage";
struct TreatyPassagePlaceBinding { StableId place_id,jurisdiction_id; };
// 作者逐场所声明可授予通行的辖区；目录本身从不产生一份已生效条约。
struct TreatyPassageProvision {
    StableId id,terms_id;
    std::vector<TreatyPassagePlaceBinding> places;
    std::vector<StableId> npc_ratifier_office_ids;
};
struct TreatyPassageCatalog {
    Integer version=1;bool enabled=false;
    OrderedTable<TreatyPassageProvision> provisions;
};
// 冻结真实会话的两名参与人；会话结束不抹去接收文书的过去，后来换任不继承知情。
struct TreatyPassageContact {
    StableId actor_id,target_actor_id,session_actor_id,session_id,place_id,provider_actor_id,role_id;
    std::optional<StableId> context_kind;
};
struct TreatyPassageDelivery {
    StableId id,offer_id,accepted_reply_id,source_fact_id;
    Integer tick=0,terms_revision=1;
    TreatyPassageContact contact;
    ActingAuthority recipient_authority;
    StableId recipient_grant_id;
};
enum class TreatyPassageSide { grantor,beneficiary };
// 一份签署只约束签署方；另一方在场者仅见证，仍须另提交自己的明确签署。
struct TreatyPassageSignature {
    StableId id,offer_id,accepted_reply_id,terms_id,knowledge_source_fact_id,prior_signature_id;
    TreatyPassageSide side=TreatyPassageSide::grantor;
    ActingAuthority authority,witness_authority;
    StableId grant_id,witness_grant_id;
    Integer tick=0,terms_revision=1;
    TreatyPassageContact contact;
    std::vector<TreatyPassageControlSnapshot> controls;
};
struct TreatyPassageState {
    Integer version=1;
    OrderedTable<TreatyPassageDelivery> deliveries;
    OrderedTable<TreatyPassageSignature> signatures;
};
struct TreatyPassagePresentCommand {
    StableId offer_id,accepted_reply_id,source_fact_id,target_actor_id;
    Integer terms_revision=1;
};
struct TreatyPassageSignCommand {
    ActingAuthority authority;
    StableId offer_id,accepted_reply_id,knowledge_source_fact_id,expected_prior_signature_id,target_actor_id;
    Integer terms_revision=1;
};
// 所有借口绑定当前完整候选；不缓存Office、历史向量或optional的内部地址。
struct TreatyPassageContext {
    DiplomaticPresentRead diplomacy;
    const TreatyPassageCatalog& catalog;
    const TreatyPassageState& state;
    const TreatyPassageControlRead& controls;
    const TaskEnvoyRead* envoy_tasks=nullptr;
};
struct TreatyPassagePresentQuote { WorldIdentity identity;TreatyPassageDelivery delivery;FaceToFaceContactProof contact; };
struct TreatyPassageSignQuote { WorldIdentity identity;TreatyPassageSignature signature;FaceToFaceContactProof contact; };
// 生效与到期只由两份原签署和原条款推导，不保存可被单改的active标志。
struct TreatyPassageAgreement {
    StableId offer_id,terms_id,grantor_signature_id,beneficiary_signature_id;
    Integer effective_from_tick=0,expires_tick=0,notice_from_tick=0;
};
Status validate_treaty_passage_catalog(const CommonCatalog&,const OrganizationCatalog&,const PoliticsCatalog&,const DiplomacyCatalog&,const TreatyPassageCatalog&);
Status validate_treaty_passage_state(const TreatyPassageContext&);
Outcome<std::optional<TreatyPassageAgreement>> read_treaty_passage_agreement(const TreatyPassageContext&,const StableId& offer_id);
Outcome<TreatyPassagePresentQuote> preview_treaty_passage_presentation(const TreatyPassageContext&,const StableId& actor,const TreatyPassagePresentCommand&);
Outcome<TreatyPassageSignQuote> preview_treaty_passage_signature(const TreatyPassageContext&,const StableId& actor,const TreatyPassageSignCommand&);
Outcome<StableId> present_treaty_passage_in_transaction(CommonStore&,TreatyPassageState&,const TreatyPassageContext&,const StableId& actor,const TreatyPassagePresentCommand&,const TreatyPassagePresentQuote&);
Outcome<StableId> sign_treaty_passage_in_transaction(CommonStore&,TreatyPassageState&,const TreatyPassageContext&,const StableId& actor,const TreatyPassageSignCommand&,const TreatyPassageSignQuote&);
BoundaryValue encode_treaty_passage_catalog(const TreatyPassageCatalog&);
Outcome<TreatyPassageCatalog> decode_treaty_passage_catalog(const BoundaryValue&);
BoundaryValue encode_treaty_passage_state(const TreatyPassageState&);
Outcome<TreatyPassageState> decode_treaty_passage_state(const BoundaryValue&);
Status visit_treaty_passage_checkpoint(CodecVisitor&,TreatyPassageState&);
BoundaryValue encode_treaty_passage_present(const TreatyPassagePresentCommand&);
Outcome<TreatyPassagePresentCommand> decode_treaty_passage_present(const BoundaryValue&);
BoundaryValue encode_treaty_passage_sign(const TreatyPassageSignCommand&);
Outcome<TreatyPassageSignCommand> decode_treaty_passage_sign(const BoundaryValue&);
}

// S6 P15.6/22.1：准入与关费只读裁决；实际付款和跨界仍由S1/Common同候选提交。
#pragma once
#include "resident/domains/travel/spatial_carrier.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
// 明版原依据：/1 不写这些尾字段，/2 必须按实际请求保存，不由空条约引用推导。
struct ExistingOriginalPassageBasis { StableId policy_id; Integer policy_version=0; StableId catalog_digest; };
struct ActivatedTreatyPassageBasis { SourceRef activation_ref; StableId term_id; };
struct TreatySafeExitPassageBasis {
 StableId binding_id; Integer policy_version=0; SourceRef entry_receipt_ref;
 std::vector<SourceRef> current_manifest_successor_refs; WorldMoment exit_until;
};
using PoliticalPassageAuthorityBasis=std::variant<ExistingOriginalPassageBasis,ActivatedTreatyPassageBasis,TreatySafeExitPassageBasis>;
enum class PassageAdmission { allowed,denied,requires_presented_document };
enum class PassageTariffKind { fixed_per_crossing,per_person,per_vessel,per_weight_unit };
struct PassageTariff {
    SourceRef source; PassageTariffKind kind=PassageTariffKind::fixed_per_crossing;
    StableId currency_id,payee_wallet_id; Integer amount_per_unit=0,weight_unit=0;
};
struct PassageToll {StableId currency_id;Integer amount=0;StableId payee_wallet_id;SourceRef tariff_source_ref;};
struct PoliticalPassagePolicyRead {
    WorldIdentity identity;SourceRef admission_source;
    PassageAdmission admission=PassageAdmission::denied;
    std::optional<SourceRef> exemption_source;
    std::optional<PassageTariff> treaty_tariff,local_tariff;
    std::vector<SourceRef> authority_basis_refs,treaty_term_refs,local_delivery_source_refs;
    StableId visible_reason_code;
    PoliticalPassageAuthorityBasis authority_basis=ExistingOriginalPassageBasis{};
};
struct PoliticalPassageDecision {
    WorldIdentity identity;PassageBoundaryRef boundary;SubjectRef subject;StableId mode;
    Integer entry_tick=0;PassageAdmission admission=PassageAdmission::denied;
    std::optional<PassageToll> toll;
    std::vector<SourceRef> authority_basis_refs,treaty_term_refs,local_delivery_source_refs;
    SourceRef admission_source;PassageRequester requester;Integer source_schema_version=1;
    StableId visible_reason_code;
    // 只在本次热请求有效，S6持久事实的paired visitor明确不写此成员。
    std::shared_ptr<const ReadStamp> read_stamp;
    PassagePurposeContext purpose_context=OrdinaryPassageContext{};
    PoliticalPassageAuthorityBasis authority_basis=ExistingOriginalPassageBasis{};
    PassageWindow requested_window;
};
bool same_political_passage_purpose(const PassagePurposeContext&,const PassagePurposeContext&);
Status validate_political_passage_authority_payload(const PoliticalPassageDecision&);
class PoliticalPassageSources {
public:
    virtual ~PoliticalPassageSources()=default;
    // 真S1 execution/manifest及S5已接受家庭policy/phase反读；无原源不能回success。
    virtual Status verify_actual_request(const PassageRequest&,const TransportManifestRead&) const=0;
    // 真当前控制、条约/免税、文书/本地送达与原合法费率。无政治能力是错误，不是allowed。
    virtual Outcome<PoliticalPassagePolicyRead> read_current_policy(const PassageRequest&,const TransportManifestRead&) const=0;
    virtual Status verify_tariff_payee(const PassageBoundaryRef&,const PassageTariff&) const=0;
};
struct PoliticalPassageContext {
    WorldIdentity identity;ReadMode read_mode;
    const CommonRead& common;const ReadStampAuthority& stamps;
    const DomainReadVersionSource& versions;const PoliticalPassageSources& sources;
    Integer selected_source_schema=1;
};
Outcome<PoliticalPassageDecision> evaluate_political_passage(const PoliticalPassageContext&,const PassageRequest&,const TransportManifestRead&);
}

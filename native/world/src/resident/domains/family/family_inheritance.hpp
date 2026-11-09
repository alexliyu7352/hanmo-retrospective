// 独立遗产受领账：真实获知、本人接受和实际清算各自引用原来源，不继承控制或公权。
#pragma once
#include "bequest_disclosure_source.hpp"
#include "inheritance_personal_assets.hpp"
#include "family_inheritance_sources2.hpp"
namespace hanmo::resident {
struct FamilyInheritancePolicy { bool enabled=false;Integer selected_version=1; };
struct FamilyBequestDisclosure { StableId kind="family_bequest_disclosed";BequestDisclosureSource source;FieldOrder field_order; };
// S3真实文书交付的薄DTO；当前identity只作读戳，永久事实保存精确原ID和原时刻。
struct FamilyReceivedDeathNotice {
    WorldIdentity identity;StableId scenario_id,binding_fact_id,task_id,source_lot_id,handoff_fact_id,death_fact_id;
    StableId decedent_actor_id,sender_actor_id,recipient_actor_id;Integer issued_tick=0,received_tick=0;
};
using FamilyReceivedDeathNoticeRead=std::function<Outcome<std::optional<FamilyReceivedDeathNotice>>(const StableId& observer,const StableId& binding_fact_id)>;
struct FamilyInheritanceAcceptance {
    StableId kind="family_bequest_accepted",declaration_id,disclosure_fact_id,beneficiary_actor_id;
    StableId death_fact_id,notice_binding_fact_id,notice_task_id,notice_source_lot_id,notice_handoff_fact_id;
    Integer notice_received_tick=0,tick=0;
    std::optional<FamilyInheritanceKnowledge2> known_death;std::optional<WorldMoment> occurred_at;FieldOrder field_order;
};
// 每次交割永久保存原资产腿和新批次ID；原权利只减量，不以同类新品回填失去的旧批次。
struct FamilyInheritanceSettlement {
    StableId kind="family_inheritance_settled",acceptance_fact_id,place_id,settlement_id;
    SuccessionPersonalTerms transfer;std::vector<StableId> destination_lot_ids;
    Integer tick=0,source_balance_before=0,destination_balance_before=0;std::optional<WorldMoment> occurred_at;
    FieldOrder field_order;
};
// 受领决定不含资金或批次，后续清算只引用这份已经真实接受的原范围。
struct FamilyInheritanceState {
    OrderedTable<FamilyBequestDisclosure> disclosures;
    OrderedTable<FamilyInheritanceAcceptance> acceptances;
    OrderedTable<FamilyInheritanceSettlement> settlements;
    FieldOrder field_order;
};
struct FamilyInheritanceAcceptanceQuote { WorldIdentity identity;FamilyInheritanceAcceptance decision; };
Status validate_family_inheritance(const CommonStore&,const FamilyBequestState&,const FamilyDeathState&,const InteractionCatalog&,const FamilyBequestPlayerRead&,const FamilyReceivedDeathNoticeRead&,const FamilyInheritancePolicy&,const FamilyInheritanceState&,const InheritancePrivateClearanceRead& clearance={},const FamilyInheritanceSources2* sources=nullptr);
Outcome<StableId> disclose_family_bequest(CommonStore&,const FamilyBequestState&,const FamilyDeathState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const FamilyBequestPlayerRead&,const FamilyReceivedDeathNoticeRead&,const FamilyInheritancePolicy&,FamilyInheritanceState&,const BequestDisclosureQuote&,const InheritancePrivateClearanceRead& clearance={},const FamilyInheritanceSources2* sources=nullptr);
// 当前本人只从已公开条款与本人实际收信记录提出接受；远方全表没有查询出口。
Outcome<FamilyInheritanceAcceptanceQuote> preview_family_inheritance_acceptance(const CommonStore&,const FamilyBequestState&,const FamilyDeathState&,const InteractionCatalog&,const FamilyBequestPlayerRead&,const FamilyReceivedDeathNoticeRead&,const FamilyInheritancePolicy&,const FamilyInheritanceState&,const StableId& disclosure_fact_id,const StableId& notice_binding_fact_id,const InheritancePrivateClearanceRead& clearance={},const FamilyInheritanceSources2* sources=nullptr);
Outcome<StableId> accept_family_inheritance(CommonStore&,const FamilyBequestState&,const FamilyDeathState&,const InteractionCatalog&,const FamilyBequestPlayerRead&,const FamilyReceivedDeathNoticeRead&,const FamilyInheritancePolicy&,FamilyInheritanceState&,const FamilyInheritanceAcceptanceQuote&,const InheritancePrivateClearanceRead& clearance={},const FamilyInheritanceSources2* sources=nullptr);
// 有限本人知识索引，声明未公开时不会从全遗赠账推导受益资格。
std::vector<StableId> known_bequest_disclosures(const FamilyInheritanceState&,const StableId& observer);
// 未登记前仍是独立新段源码；旧Family/声明/Task visitor均不增加字段。
Outcome<FamilyInheritancePolicy> decode_family_inheritance_policy(const BoundaryValue&);
Outcome<FamilyInheritancePolicy> decode_family_inheritance_policy_v2(const BoundaryValue&);
Outcome<FamilyInheritanceState> decode_family_inheritance(const BoundaryValue&);
BoundaryValue encode_family_inheritance(const FamilyInheritanceState&);
Status visit_family_inheritance_checkpoint(CodecVisitor&,FamilyInheritanceState&);
Outcome<FamilyInheritanceState> decode_family_inheritance_v2(const BoundaryValue&);
BoundaryValue encode_family_inheritance_v2(const FamilyInheritanceState&);
Status visit_family_inheritance_checkpoint_v2(CodecVisitor&,FamilyInheritanceState&);
// 按Common实际顺序扣减既有交割；before_fact用于冷验证某一笔之前真实剩余。
Outcome<InheritancePersonalEntitlement> inheritance_personal_entitlement(const CommonStore&,const FamilyBequestState&,const FamilyInheritanceState&,const StableId& acceptance_fact_id,const StableId& before_fact_id={});
Status validate_family_inheritance_settlements(const CommonStore&,const FamilyBequestState&,const FamilyDeathState&,const InteractionCatalog&,const FamilyBequestPlayerRead&,const FamilyInheritanceState&,const InheritancePrivateClearanceRead&,const FamilyInheritanceSources2* sources=nullptr,Integer selected_version=1);
Outcome<InheritancePersonalQuote> preview_family_inheritance_settlement(const CommonStore&,const FamilyBequestState&,const FamilyDeathState&,const InteractionCatalog&,const FamilyBequestPlayerRead&,const FamilyReceivedDeathNoticeRead&,const FamilyInheritancePolicy&,const FamilyInheritanceState&,const StableId& acceptance_fact_id,const InheritancePersonalSelection&,const InheritancePrivateClearanceRead&,const FamilyInheritanceSources2* sources=nullptr);
Outcome<FamilyInheritanceAcceptanceQuote> preview_family_inheritance_acceptance_v2(const CommonStore&,const FamilyBequestState&,const FamilyDeathState&,const InteractionCatalog&,const FamilyBequestPlayerRead&,const FamilyInheritancePolicy&,const FamilyInheritanceState&,const StableId& disclosure_fact_id,const ActorKnownDeathReceiptRef&,const FamilyInheritanceSources2&,const InheritancePrivateClearanceRead& clearance={});
// 原财富与本域交割账共享保存点；资产已写后来源失败仍完整回退，不留半份遗产。
Outcome<StableId> settle_family_inheritance(CommonStore&,const FamilyBequestState&,const FamilyDeathState&,const InteractionCatalog&,const FamilyBequestPlayerRead&,const FamilyReceivedDeathNoticeRead&,const FamilyInheritancePolicy&,FamilyInheritanceState&,const InheritancePersonalQuote&,const InheritancePrivateClearanceRead&,const FamilyInheritanceSources2* sources=nullptr);
// Borrow the original immutable row/header only, for P10/source registration.
// Full Family inheritance validation still runs at install/restore/publication.
struct FamilyInheritanceActualFact2 {const FactRecord* header=nullptr;const StableId* actor_id=nullptr;const WorldMoment* occurred_at=nullptr;};
Outcome<FamilyInheritanceActualFact2> read_family_inheritance_actual_fact2(const CommonStore&,const FamilyInheritancePolicy&,const FamilyInheritanceState&,const SourceRef&,const WorldMoment& actual_now);
}

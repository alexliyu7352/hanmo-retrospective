// 雇用段保存条款、本人决定和真实到岗引用；应付、余数、付款仍唯一写在LE原账。
#pragma once
#include "estate_employment_contract.hpp"
#include "../estate_model.hpp"
#include "../../local_economy/local_economy_model.hpp"
#include "resident/domains/interaction/dialogue_provider_source.hpp"
namespace hanmo::resident {
struct FamilyDeathState;struct FamilyBirthState;class EstateAssetTitleRead;
// 作者绑定原庄园和原工资率，并分别指明雇主邀约、受雇本人答复的真实对白节点。
struct EstateEmploymentPolicy {
    StableId id,estate_id,job,rate_profile,offer_dialogue_id,offer_node_id,worker_dialogue_id,worker_node_id;
    Integer service_ticks=0,offer_valid_ticks=0;
    StableId provider_offer_dialogue_id,provider_offer_node_id,provider_reply_dialogue_id,provider_reply_node_id,provider_reply_decision;
};
struct EstateEmploymentCatalog { Integer version=1;OrderedTable<EstateEmploymentPolicy> policies; };
// 冷史保留当面节点身份，不把报价的revision/epoch冒充永远有效的权力。
struct EstateEmploymentMeeting {
    StableId actor_id,provider_actor_id,session_id,dialogue_id,node_id,node_kind,place_id,speaker_actor_id;
};
struct EstateEmploymentContract {
    StableId id,status="offered",acceptance_fact_id,terminal_fact_id,obligation_id;
    EstateEmploymentTerms terms;EstateEmploymentMeeting offered_at;
    Integer offered_tick=0,accepted_tick=-1,through_tick=-1;bool offer_from_provider=false;
};
struct EstateEmploymentDecisionFact {
    StableId id,contract_id,actor_id,decision;Integer tick=0;
    std::optional<EstateEmploymentMeeting> meeting;
    // 已赚分数工资仅在首次终止时转为原LE一笔应付；不和未赚预约混同。
    StableId terminal_arrear_id;bool provider_declared=false;StableId death_fact_id;
};
struct EstateEmploymentService {
    StableId id,contract_id,production_fact_id,arrear_id;
    Integer tick=0,remainder_before=0,remainder_after=0;
};
struct EstateEmploymentState {
    Integer version=1,initialized_tick=0,last_step_tick=0;
    OrderedTable<EstateEmploymentContract> contracts;
    OrderedTable<EstateEmploymentDecisionFact> decisions;
    OrderedTable<EstateEmploymentService> services;
};
// 原状态对象必须同候选；内部读每次即时借用，不跨事务保存table元素指针。
struct EstateEmploymentContext {
    const CommonStore& common;const CommonCatalog& common_catalog;
    const EstateState& estates;const EstateCatalog& estate_catalog;
    const LocalEconomyState& economy;const LocalEconomyCatalog& economy_catalog;
    const EstateEmploymentState& state;const EstateEmploymentCatalog& catalog;
    const ActivityRead& activity;const EstateAccess* estate_access=nullptr;
    const DynamicStepInput* phase=nullptr;
    const InteractionCatalog* interaction_catalog=nullptr;
    const FamilyDeathState* life=nullptr;
    // 只借同候选已验证出生账；冷成年资格在完整Birth后调用，不从成熟原型倒填。
    const FamilyBirthState* births=nullptr;
    // Selected complete Estate title history; borrowed from the same candidate.
    // Legacy composition leaves it absent and retains its original ownership gate.
    const EstateAssetTitleRead* estate_titles=nullptr;
};
struct EstateEmploymentMeetingContext {
    const InteractionState& state;const InteractionCatalog& catalog;const InteractionAccess& access;
};
struct EstateEmploymentQuote {
    WorldIdentity source_identity;StableId actor_id;EstateEmploymentCommand command;
    EstateEmploymentTerms terms;std::optional<AuthoredNodeSourceProof> meeting;
    std::optional<AuthoredProviderSourceProof> provider;StableId resolved_decision;
};
struct EstateEmploymentReceipt { StableId contract_id,fact_id,obligation_id;std::vector<StableId> wage_receipt_ids; };
// 全量校验按真实事实顺序重算工资/余数与预约；旧无段入口不能接收本段义务。
// 早位公责排除只核原会面/工资/资产/死亡全来源；不授成年资格，晚位必须仍调用完整门。
Status validate_estate_employment_original_sources(const EstateEmploymentContext&);
Status validate_estate_employment(const EstateEmploymentContext&);
Outcome<EstateEmploymentQuote> preview_estate_employment(const EstateEmploymentContext&,const EstateEmploymentMeetingContext&,const StableId& actor,const EstateEmploymentCommand&);
Outcome<EstateEmploymentReceipt> apply_estate_employment(CommonStore&,EstateState&,LocalEconomyState&,EstateEmploymentState&,const EstateEmploymentContext&,const EstateEmploymentMeetingContext&,const StableId& actor,const EstateEmploymentCommand&);
// 中央完整候选/NPC已有Common保存点；晚失败由外层整World回滚，不再次开保存点。
Outcome<EstateEmploymentReceipt> apply_estate_employment_in_candidate(CommonStore&,EstateState&,LocalEconomyState&,EstateEmploymentState&,const EstateEmploymentContext&,const EstateEmploymentMeetingContext&,const StableId& actor,const EstateEmploymentCommand&,const EstateEmploymentQuote&);
// 只消费本次严格from→to的EstateProductionFact；不从过去到岗补发未来工资。
Outcome<LocalEconomyEffects> step_estate_employment(CommonStore&,EstateState&,LocalEconomyState&,EstateEmploymentState&,const EstateEmploymentContext&,const DynamicStepInput&);
// 仅在clock_commit和S4真实死亡登记之后调用；不把preclock alive=false猜成过去死因。
Outcome<LocalEconomyEffects> close_deceased_estate_employment(CommonStore&,EstateState&,LocalEconomyState&,EstateEmploymentState&,const EstateEmploymentContext&);
Outcome<std::optional<EstateEmploymentSource>> read_estate_employment_source(const EstateEmploymentContext&,const StableId& observer,const StableId& contract);
// 原页面通过此有限同候选适配器读取；页不持整个Estate/LE或自行扫描合同账。
class EstateEmploymentAccess final:public EstateEmploymentRead {
public:
    EstateEmploymentAccess(const EstateEmploymentContext& context,const EstateEmploymentMeetingContext& meeting):context_(context),meeting_(meeting){}
    Outcome<std::optional<EstateEmploymentSource>> estate_employment_source(const StableId&,const StableId&)const override;
    Outcome<std::vector<StableId>> estate_employment_contract_ids(const StableId&,Integer offset,Integer limit)const override;
    Outcome<std::vector<EstateEmploymentPolicySource>> estate_employment_policies(const StableId&,const StableId&)const override;
private:
    const EstateEmploymentContext& context_;const EstateEmploymentMeetingContext& meeting_;
};
// 原管理命令不能绕过仍有效的岗位合同；终止合同后才可改派其约定职责。
Status guard_estate_employment_management(const EstateEmploymentContext&,const EstateCommand&);
Outcome<EstateEmploymentCatalog> decode_estate_employment_catalog(const BoundaryValue&);
Outcome<EstateEmploymentState> decode_estate_employment_state(const BoundaryValue&);
// 仅显式新段给旧Estate冷解码器路由恰一已解码载荷；旧入口不得吞就业事实。
Outcome<EstateState> decode_estate_seed(const BoundaryValue&,const EstateEmploymentState*);
class ProjectionWriter;
Status emit_estate_employment_state_fields(ProjectionWriter&,const EstateEmploymentState&);
BoundaryValue encode_estate_employment_state(const EstateEmploymentState&);
Status visit_estate_employment_checkpoint(CodecVisitor&,EstateEmploymentState&);
Outcome<EstateEmploymentState> make_initial_estate_employment(const CommonStore&,const EstateState&,const LocalEconomyState&);
}

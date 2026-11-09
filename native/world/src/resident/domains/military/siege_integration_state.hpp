// 围城整编的独立typed持久合同；提议不可变，尝试只引用来源，不复制军事或人口账。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
// 条款交接提交时尚存的合法公产；私人财产与旧债不转移，现任单位指挥按明确条款保留。
struct SiegeIntegrationTerms {
    StableId kind="preserve_units_v1";
    bool preserve_unit_command=true,preserve_private_property=true;
    bool transfer_current_public_assets=true,transfer_prisoner_custody=true;
    bool recipient_future_service_responsibility=true,retain_original_arrears=true;
    std::vector<StableId> population_policy_ids;
    FieldOrder field_order;
};
// 任官快照保留原始任命事实及启用位；不能把任官本身作为接受凭据。
struct SiegeIntegrationAppointment {
    StableId id,holder_actor_id,appointment_id;
    bool enabled=false;
    FieldOrder field_order;
};
// 五类兵员及看管/借调映射来自现有单位；士气、疲劳和粮钱余额不参与语义身份。
struct SiegeIntegrationUnit {
    StableId id,source_garrison_id,organization_id,army_id,commander_actor_id,troop_type_id;
    std::vector<StableId> member_ids,fit,wounded,dead,captured,deserted;
    std::vector<std::pair<StableId,StableId>> captors,retainer_sources;
    FieldOrder field_order;
};
// 每位所需军官绑定精确单位、原编组及全部实际任命，禁止多数替代或代签。
struct SiegeIntegrationOfficer {
    StableId actor_id,army_id,formation_source_id;
    std::vector<StableId> unit_ids;
    std::vector<SiegeIntegrationAppointment> appointment_sources;
    FieldOrder field_order;
};
// 私人借调仅保留来源；是否可合法续接由资产和私人产业owner另行验证。
struct SiegeIntegrationRetainer {
    StableId actor_id,estate_id,source_id,unit_id;
    FieldOrder field_order;
};
// 原GD合同的军团投影；保全精确位置、原军令、公共账户、军官、俘虏与逐部分类。
struct SiegeIntegrationArmy {
    StableId id,organization_id,campaign_id,formation_source_id,commander_actor_id,office_id,garrison_id,inventory_id,wallet_id;
    Location location;
    std::vector<StableId> unit_ids,member_actor_ids,prisoner_ids;
    std::vector<SiegeIntegrationRetainer> retainers;
    std::vector<SiegeIntegrationUnit> units;
    std::vector<SiegeIntegrationOfficer> officers;
    FieldOrder field_order;
};
// id是永久来源身份，proposal_key仅摘要稳定语义；时间推进和另一人的决定不改变该摘要。
struct SiegeIntegrationProposal {
    StableId id,proposal_key,siege_id,campaign_id,stronghold_id,proposer_actor_id,actor_action_source_id;
    Integer created_tick=0;
    SiegeIntegrationArmy recipient;
    std::vector<SiegeIntegrationArmy> defender_armies;
    std::vector<SiegeIntegrationOfficer> required_officers;
    SiegeIntegrationTerms terms;
    FieldOrder field_order;
};
// 真实中央actor命令的永久证明；与决定事实双向绑定，不读取短命命令缓存来证明接受。
struct SiegeIntegrationActorAction {
    StableId id,command,actor_id,proposal_id,proposal_key,decision,decision_source_id,policy_reason;
    Integer tick=0;
    FieldOrder field_order;
};
// 接受或拒绝都记录本人实际命令和当时的职责来源，保存后不可改签或重复签署。
struct SiegeOfficerDecision {
    StableId id,proposal_id,proposal_key,actor_id,decision,actor_action_source_id,legal_basis,policy_reason;
    Integer tick=0;
    SiegeIntegrationOfficer officer;
    FieldOrder field_order;
};
// 当前尝试引用永久记录；rejected/stale/cancelled/completed均不删除已发生的决定。
struct SiegeIntegrationAttempt {
    StableId proposal_id,status="pending",reason,completion_source_id;
    std::vector<StableId> decision_source_ids;
    FieldOrder field_order;
};
// 可选作者政策只限定本人接受的组织/条款/既有意愿门槛；无政策拒绝，临时占用等待。
struct SiegeIntegrationOfficerPolicy {
    StableId actor_id;
    std::vector<StableId> allowed_recipient_organization_ids,allowed_terms;
    Integer minimum_willingness=50;
    bool require_unit_command=true,require_private_property=true;
    FieldOrder field_order;
};
// 本域只拥有提议与决定，不持有第二份资产、人口、任官或当前军籍。
struct SiegeIntegrationState {
    std::vector<SiegeIntegrationProposal> proposals;
    std::vector<SiegeIntegrationActorAction> actor_actions;
    std::vector<SiegeOfficerDecision> decisions;
    std::vector<SiegeIntegrationAttempt> attempts;
};
// 能力由中央选择的情景显式激活；政策缺失不补全默认同意。
struct SiegeIntegrationCatalog {
    bool enabled=false;
    std::vector<SiegeIntegrationOfficerPolicy> officer_policies;
    SiegeIntegrationTerms default_terms;
};
// 命令参数不能带actor、签名来源或预先构造的接受记录，调用者身份只来自中央入口。
struct SiegeIntegrationPropose { StableId siege_id;SiegeIntegrationTerms terms;FieldOrder field_order; };
struct SiegeIntegrationDecide { StableId proposal_id,proposal_key,decision;FieldOrder field_order; };
struct SiegeIntegrationCancel { StableId proposal_id,proposal_key;FieldOrder field_order; };
using SiegeIntegrationCommand=std::variant<SiegeIntegrationPropose,SiegeIntegrationDecide,SiegeIntegrationCancel>;
// 写命令返回实际创建来源；只读政策结果仍须交给同一个actor命令入口执行。
struct SiegeIntegrationEffect { StableId proposal_id,proposal_key,actor_action_source_id,decision_source_id,status;StableId completion_source_id{}; };
struct SiegeIntegrationNpcDecision { StableId actor_id,decision,reason; };
} // 保存以上值不等于取得交接资格；最终资产候选必须再次验证全部来源。

// 生前自愿接续的明确条款；关系与本人接受分开，死后控制同意不属于本目录。
#pragma once
#include "voluntary_relation_source.hpp"
#include "succession_consent_source.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_contract.hpp"
namespace hanmo::resident {
struct VoluntaryPathDefinition {
    StableId id,template_id,accept_node_id,withdraw_node_id;
    VoluntaryRelationKind relation_kind=VoluntaryRelationKind::adoption;
    // 门人路径只认这一门真实已接受师承；其余路径必须为空。
    StableId disciple_skill_id;
    Integer terms_version=1;
    SuccessionPersonalSelection personal;
};
struct VoluntaryPathCatalog {
    bool enabled=false;
    OrderedTable<VoluntaryPathDefinition> definitions;
};
using VoluntaryPathTemplateRead=std::function<Outcome<KnownPersonalDialogueTemplate>(const StableId&)>;
// 同一Prepared目录中，其他已选本人声明（死后许可、收养、师承等）的精确原节点。
// 返回true表示该句已承担别的语义；构造方必须枚举完整已选目录，不能从历史账推断。
using VoluntaryOtherDeclarationRead=std::function<Outcome<bool>(const StableId& dialogue,const StableId& node)>;
// family_voluntary_paths/1 与 voluntary_succession/2 是World准入时的显式配对。
// 本口只读目录，不登记Family格式、不把仅声明目录当成本人同意。
Outcome<VoluntaryPathCatalog> decode_voluntary_path_catalog(const BoundaryValue&);
// /2保原岗位条款可选并存；空旧request_ids仅在本显式profile合法。
Outcome<SuccessionConsentCatalog> decode_living_succession_catalog(const BoundaryValue& config,const BoundaryValue& original_definitions);
Status validate_voluntary_path_catalog(const VoluntaryPathCatalog&,const SuccessionConsentCatalog&,
    const InteractionCatalog&,const VoluntaryPathTemplateRead&,const VoluntaryOtherDeclarationRead&);
const char* voluntary_relation_kind_name(VoluntaryRelationKind);
Outcome<VoluntaryRelationKind> decode_voluntary_relation_kind(const StableId&);
}

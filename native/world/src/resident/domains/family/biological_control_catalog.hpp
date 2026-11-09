// 亲生控制政策只选择本人接受/撤回节点，关系与每次真实会面均在原业务来源中重验。
#pragma once
#include "legacy_control_consent.hpp"
namespace hanmo::resident {
enum class BiologicalControlMeetingKind { installed_candidate,born_personal_template };
struct BiologicalControlDefinition {
    StableId id,predecessor_actor_id,successor_actor_id,dialogue_id,template_id,accept_node_id,revoke_node_id;
    BiologicalControlMeetingKind meeting_kind=BiologicalControlMeetingKind::installed_candidate;
    Integer terms_version=1;
};
struct BiologicalControlCatalog {bool enabled=false;OrderedTable<BiologicalControlDefinition> definitions;};
// 仅投影S3已核模板目录的不可变三引用，不持会话/对白副本或动态人物初态。
struct BiologicalControlTemplateIdentity {StableId template_id,prototype_id,dialogue_id;};
using BiologicalControlTemplateRead=std::function<Outcome<BiologicalControlTemplateIdentity>(const StableId& template_id)>;
Outcome<BiologicalControlCatalog> decode_biological_control_catalog(const BoundaryValue&);
// 调用方须先核所选S3原模板目录；空回调仅允许没有动态模板的纯Installed子集。
Status validate_biological_control_catalog(const BiologicalControlCatalog&,const LegacyControlConsentCatalog&,const CommonCatalog&,const InteractionCatalog&,const BiologicalControlTemplateRead& templates={});
}

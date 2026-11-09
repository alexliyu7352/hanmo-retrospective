// 旁支本人认可与控制许可使用不同的明确声明；血缘及打开私人会话均不自动授予二者。
#pragma once
#include "legacy_control_consent.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_contract.hpp"
namespace hanmo::resident {
struct CollateralControlDefinition {
    StableId id,template_id,recognize_node_id,accept_node_id,revoke_node_id;
    Integer terms_version=1;
};
struct CollateralControlCatalog {bool enabled=false;OrderedTable<CollateralControlDefinition> definitions;};
// 原S3目录已全验后，从同一PreparedCatalog投影唯一实际模板，不创建另一份模板账。
using CollateralControlTemplateRead=std::function<Outcome<KnownPersonalDialogueTemplate>(const StableId&)>;
Outcome<CollateralControlCatalog> decode_collateral_control_catalog(const BoundaryValue&);
Status validate_collateral_control_catalog(const CollateralControlCatalog&,const LegacyControlConsentCatalog&,const InteractionCatalog&,const CollateralControlTemplateRead&);
}

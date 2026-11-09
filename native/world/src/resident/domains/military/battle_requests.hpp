// 部将请求只产生统帅的有限裁定候选；真实权源、幂等回执与原子计划发布由世界层负责。
#pragma once
#include "battle_model.hpp"
namespace hanmo::resident {
// 一轮一次为本节点明确的开发规则，作者可配置；不是从历史制度推导出的强制上限。
struct MilitaryRequestPolicy { Integer per_actor_round_limit=1; };
// 三种意图独立保存；request_key供世界层原幂等结果复用，不能以新键重放同一轮额度。
struct MilitaryBattleRequest { StableId battle_id,request_key,request_kind,unit_id,target_unit_id;bool fields_valid=true;FieldOrder field_order; };
// 拒绝是正常裁定；接受只替换一条原本由AI控制的合法友军计划。
struct MilitaryRequestDecision { bool accepted=false;StableId request_kind,reason,response_unit_id;MilitaryOrder response_order;FieldOrder field_order; };
// authorized_ai_units必须来自原军级权源，不能直接取用户输入；纯门仍拒敌军、本人与重复名单。
Outcome<MilitaryRequestDecision> consider_military_request(const MilitaryBattleRecord&,const MilitaryBattleCatalog&,const MilitaryBattleRequest&,const std::vector<StableId>& authorized_ai_units,const MilitaryDamageSources& = {});
}

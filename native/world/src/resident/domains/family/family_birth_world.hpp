// 出生与原World的具名组合口；规则仍在Family，钱粮/人物/Action始终写同一Common候选。
#pragma once
#include "family_birth_commands.hpp"
#include "family_birth_identity.hpp"
#include "family_birth_responsibilities.hpp"
#include "family_child_care_completion.hpp"
#include "family_birth_kinship.hpp"
namespace hanmo::resident {
struct WorldBundle;struct AdvanceDetails;struct ActionCompletion;
bool family_birth_command_kind(const StableId&);
Status validate_birth_fact_header(const FamilyBirthState&,const FactRecord&);
// Common年龄和动态人物服务之前先证明选定profile及完整typed出生身份；不调用World递归。
Status validate_family_birth_identity_world(const WorldBundle&);
// 原Military/Naval、FamilyLife全史通过后才核生育本人决定、照护和新回执。
Status validate_family_birth_world(const WorldBundle&);
// clock_commit及真实死亡登记之后结算照护，发布完成→释放原Action→恢复原等待对白。
// 本口还处理未出生死亡终止、到期真实出生和成年；任一失败由整World候选回滚。
Status finish_family_birth_world(WorldBundle&,AdvanceDetails&);
// 本人零时取消也必须恢复原等待对白；消费精确原Action/终态，不授权其他action类型。
Status release_completed_child_care(WorldBundle&,const StableId& action_id,const ActionCompletion&);
}

// 原型别的轻量声明和值合同；只搬位置，不改变布局或规则。
#pragma once
#include "resident/domains/interaction/interaction_context.hpp"
#include "resident/domains/military/siege_model.hpp"
#include "resident/domains/military/battle_world_model.hpp"
#include "resident/domains/military/training_model.hpp"
#include "resident/domains/military/battle_requests.hpp"
#include "resident/domains/military/military_officer_model.hpp"
#include "resident/domains/military/military_field_command_model.hpp"

namespace hanmo::resident {
struct OfficeAccept { StableId office_id; InteractionContext context;  std::vector<StableId> field_order; };

struct CampaignAccept { StableId template_id; InteractionContext context; std::optional<StableId> recipient_army_id;  std::vector<StableId> field_order; };

struct ArmyForm { StableId order_id; std::vector<StableId> unit_ids,retainer_ids; std::optional<Integer> food_quantity,money; bool unit_ids_valid=true,retainer_ids_valid=true,food_quantity_valid=true,money_valid=true;  std::vector<StableId> field_order; };

struct CampaignDepart { StableId campaign_id;  std::vector<StableId> field_order; };

struct CampaignManage { StableId kind,campaign_id,garrison_id;std::optional<InteractionContext> context;bool garrison_valid=true;FieldOrder field_order; };

struct ArmyLogistics { StableId kind,army_id;std::optional<StableId> garrison_id;std::optional<Integer> food_quantity,money;bool garrison_valid=true,food_valid=true,money_valid=true;FieldOrder field_order; };

struct OfficeResign { StableId office_id;InteractionContext context;FieldOrder field_order; };

struct ArmyPledge { StableId order_id,estate_id;std::vector<StableId> actor_ids;bool actors_valid=true;FieldOrder field_order; };
using MilitaryCommand=std::variant<OfficeAccept,CampaignAccept,ArmyForm,CampaignDepart,MilitaryBattlePlan,MilitaryBattleClose,SiegeStart,SiegeJoin,SiegePlan,SiegeStrategy,SiegeClose,CampaignManage,ArmyLogistics,OfficeResign,ArmyPledge,MilitaryTrain,MilitaryTrainCancel,MilitaryDrillCommand,MilitaryBattleRequest,MilitaryOfficerAssign,MilitaryFieldCommandAssign,MilitaryTacticPracticeClaim>;
} // namespace hanmo::resident

// 固定5×4军阵操练：三个抽象组和四次布置，不生成真实士兵、战损或世界奖励。
#pragma once
#include "battle_model.hpp"
namespace hanmo::resident {
struct DrillPending { StableId group_id;HexPoint target;FieldOrder field_order; };
struct DrillMove { StableId group_id;HexPoint from,target;FieldOrder field_order; };
struct DrillGroup { StableId id,kind,text_key,text;HexPoint origin;std::vector<HexPoint> targets; };
struct DrillSupplyGoal { StableId group_id,text_key,text;HexPoint point;std::vector<HexPoint> guard_cells;FieldOrder field_order; };
struct DrillPassageGoal { StableId group_id,text_key,text;std::vector<HexPoint> cells;FieldOrder field_order; };
struct DrillScreenPair { HexPoint front,rear;FieldOrder field_order; };
struct DrillScreenGoal { StableId front_group_id,ranged_group_id,text_key,text;std::vector<DrillScreenPair> pairs;FieldOrder field_order; };
struct DrillCavalryGoal { StableId group_id,text_key,text;std::vector<HexPoint> bad_cells;FieldOrder field_order; };
struct DrillDefinition { StableId id,prompt_key,prompt,source_sha256;Integer difficulty=0;std::vector<DrillGroup> groups;DrillSupplyGoal supply;DrillPassageGoal passage;DrillScreenGoal screen;DrillCavalryGoal cavalry;std::vector<DrillPending> solution;FieldOrder objectives_field_order; };
struct DrillSnapshot { Integer skill_level=0,auxiliary_attribute=1;FieldOrder field_order; };
struct DrillObjectives { bool supply=false,passage=false,screen=false,cavalry=false;FieldOrder field_order; };
struct DrillResult { StableId completion;Integer score=0;std::optional<Integer> raw_score;Integer auto_score=0,performance_bp=10000;std::optional<DrillObjectives> objectives;std::vector<DrillMove> choices;FieldOrder field_order; };
struct DrillState { StableId format="HM-DRILL-CHALLENGE/1",definition_id,source_sha256,actor_id,source_id;FieldOrder binding_field_order;DrillSnapshot actor_snapshot;StableId seed="0",mode="automatic",phase="active";std::vector<DrillMove> moves;std::optional<DrillPending> pending;std::optional<DrillResult> result;FieldOrder field_order; };
Status validate_drill_definition(const DrillDefinition&);
Status validate_drill_state(const DrillDefinition&,const DrillState&);
Outcome<DrillState> create_drill(const DrillDefinition&,const StableId& actor,const StableId& source,const DrillSnapshot&,const StableId& seed,const StableId& mode);
Outcome<DrillState> draft_drill_move(const DrillDefinition&,const DrillState&,Integer expected_step,const StableId& group,HexPoint target);
Outcome<DrillState> confirm_drill_move(const DrillDefinition&,const DrillState&,Integer expected_step,const DrillPending&);
Outcome<DrillState> cancel_drill_pending(const DrillDefinition&,const DrillState&,Integer expected_step,const DrillPending&);
Outcome<DrillState> exit_drill(const DrillDefinition&,const DrillState&);
}

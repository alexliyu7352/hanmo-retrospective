// One root field sequence; all original payload callbacks remain unchanged.
#pragma once
#include "military_action_time_record_fields.hpp"
#include "training_exact_codec_fields.hpp"
#include "naval_repair_exact_fields.hpp"
namespace hanmo::resident::military_action_time_state_fields {
template<class Archive>Status value(Archive& c,MilitaryActionTimeState& state,Integer selected){
 if(selected!=1)return c.reject("MilitaryActionTimeSelectedSchemaRequired");
 Integer schema=selected;c.field("schema",schema);
 if(!c.status().ok)return c.status();
 if(schema!=selected)return c.reject("MilitaryActionTimeSelectedSchemaMismatch");
 c.table("dynamic_action_facts",state.dynamic_action_facts,[](auto& x,auto& row){return military_action_time_fields::value(x,row);});
 c.table("dynamic_logistics_facts",state.dynamic_logistics_facts,[](auto& x,auto& row){return military_action_time_fields::value(x,row);});
 c.table("dynamic_return_facts",state.dynamic_return_facts,[](auto& x,auto& row){return military_action_time_fields::value(x,row);});
 c.table("dynamic_training_facts",state.dynamic_training_facts,[](auto& x,auto& row){return training_exact_fields::value(x,row);});
 c.table("training_participation_spans",state.training_participation_spans,[](auto& x,auto& row){return training_exact_fields::value(x,row);});
 c.table("training_period_witnesses",state.training_period_witnesses,[](auto& x,auto& row){return training_exact_fields::value(x,row);});
 c.table("drill_freeze_sources",state.drill_freeze_sources,[](auto& x,auto& row){return training_exact_fields::value(x,row);});
 c.table("naval_repair_time_sources",state.naval_repair_time_sources,[](auto& x,auto& row){return naval_repair_exact_fields::value(x,row);});
 return c.status();
}
}

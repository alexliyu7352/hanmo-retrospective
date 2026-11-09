#include "family_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include "household_control.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident {
Outcome<HouseholdControlCatalog> decode_household_control_catalog(const BoundaryValue& value){try{
    using namespace boundary;fields(value,{"profile","initial_mandates"},{},"household_control");
    const auto profile=get_string(value,"profile","");
    if(profile!="initial_household_stewardship_v1"&&profile!="initial_household_stewardship_v2")unsupported("UnsupportedHouseholdControlProfile","household_control/profile");
    const auto& rows=array(field(value,"initial_mandates",""),"household_control/initial_mandates");
    if(rows.empty()||rows.size()>4096)fail("InvalidHouseholdControlCatalog","household_control/initial_mandates");
    HouseholdControlCatalog result;result.purchase_control_enabled=profile=="initial_household_stewardship_v2";result.market_staffing_control_enabled=profile=="initial_household_stewardship_v2";
    for(const auto& row:rows){
        fields(row,{"id","actor_id","household_group_id","source_role_id","operation","effective_from_tick","actor_accepted"},{"through_tick"},"household_control/initial_mandates");
        HouseholdControlDefinition d;d.id=get_string(row,"id","");d.actor_id=get_string(row,"actor_id","");d.household_group_id=get_string(row,"household_group_id","");d.source_role_id=get_string(row,"source_role_id","");d.operation=get_string(row,"operation","");d.effective_from_tick=get_int(row,"effective_from_tick","");d.actor_accepted=get_bool(row,"actor_accepted","");
        if(const auto* end=row.find("through_tick"))d.through_tick=integer(*end,"through_tick");
        if(!result.initial_mandates.insert(d.id,d))fail("DuplicateHouseholdControlDefinition",d.id);
    }
    return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
HouseholdControlFact decode_household_control_fact(const BoundaryValue& value){
    using namespace boundary;fields(value,{"kind","definition_id","actor_id","household_group_id","source_role_id","operation","scenario_id","recorded_tick","effective_from_tick","actor_accepted"},{"through_tick"},"household_control_fact");
    HouseholdControlFact f;f.kind=get_string(value,"kind","");f.definition_id=get_string(value,"definition_id","");f.actor_id=get_string(value,"actor_id","");f.household_group_id=get_string(value,"household_group_id","");f.source_role_id=get_string(value,"source_role_id","");f.operation=get_string(value,"operation","");f.scenario_id=get_string(value,"scenario_id","");f.recorded_tick=get_int(value,"recorded_tick","");f.effective_from_tick=get_int(value,"effective_from_tick","");f.actor_accepted=get_bool(value,"actor_accepted","");
    if(const auto* end=value.find("through_tick"))f.through_tick=integer(*end,"through_tick");f.field_order=order(value);return f;
}
Status emit_household_control_fact_fields(ProjectionWriter& out,const HouseholdControlFact& f){out.field("kind",f.kind);out.field("definition_id",f.definition_id);out.field("actor_id",f.actor_id);out.field("household_group_id",f.household_group_id);out.field("source_role_id",f.source_role_id);out.field("operation",f.operation);out.field("scenario_id",f.scenario_id);out.field("recorded_tick",f.recorded_tick);out.field("effective_from_tick",f.effective_from_tick);out.field("actor_accepted",f.actor_accepted);if(f.through_tick)out.field("through_tick",*f.through_tick);return out.order_fields(f.field_order);}
BoundaryValue encode_household_control_fact(const HouseholdControlFact& f){return family_projection::encode([&](auto& w){return emit_household_control_fact_fields(w,f);});}
Status visit_household_control_fact(CodecVisitor& c,HouseholdControlFact& f){
    c.field("kind",f.kind);c.field("definition_id",f.definition_id);c.field("actor_id",f.actor_id);c.field("household_group_id",f.household_group_id);c.field("source_role_id",f.source_role_id);c.field("operation",f.operation);c.field("scenario_id",f.scenario_id);c.field("recorded_tick",f.recorded_tick);c.field("effective_from_tick",f.effective_from_tick);c.field("actor_accepted",f.actor_accepted);c.optional("through_tick",f.through_tick,[](auto& v,auto& x){return v.field("value",x);});return common_checkpoint::strings(c,"field_order",f.field_order);
}
}

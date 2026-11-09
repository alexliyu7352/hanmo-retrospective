#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_march_codec.hpp"
#include "duty/military_duty_codec_fields.hpp"
#include "resident/domains/actor_foundation/condition_head_codec_fields.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {namespace march_codec {
template<class C>Status fields(C& c,MilitaryGroupOrderAcceptance& v){
 Integer kind=Integer(v.kind);c.field("kind",kind);if(c.status().ok&&(kind<0||kind>1))return c.reject("InvalidMilitaryMarchOrderKind");if(c.reading()&&c.status().ok)v.kind=MilitaryMarchOrderKind(kind);
 c.field("campaign_id",v.campaign_id);c.field("army_id",v.army_id);c.field("issuing_actor_id",v.issuing_actor_id);c.field("commander_actor_id",v.commander_actor_id);
 c.object("formation_source",[&](auto& x){return military_duty_codec::value(x,v.formation_source);});
 c.object("command_source",[&](auto& x){Integer tag=Integer(v.command_source.kind);x.field("kind",tag);if(x.status().ok&&(tag<0||tag>3))return x.reject("InvalidMilitaryFieldCommandSource");if(x.reading()&&x.status().ok)v.command_source.kind=MilitaryFieldCommandSourceKind(tag);x.field("id",v.command_source.id);return x.status();});
 c.object("issuing_authority",[&](auto& x){return military_duty_codec::value(x,v.issuing_authority);});
 c.object("issuing_condition_head",[&](auto& x){return actor_condition_codec::head(x,v.issuing_condition_head);});
 c.object("commander_condition_head",[&](auto& x){return actor_condition_codec::head(x,v.commander_condition_head);});
 c.field("destination_settlement_id",v.destination_settlement_id);c.optional("return_garrison_id",v.return_garrison_id,[](auto& x,auto& id){return x.field("value",id);});
 c.object("accepted_at",[&](auto& x){return exact_fields::moment(x,v.accepted_at);});
 c.sequence("original_authority_sources",v.original_authority_sources,[](auto& x,auto& ref){return military_duty_codec::value(x,ref);});
 c.optional("original_return_cause",v.original_return_cause,[](auto& x,auto& ref){return military_duty_codec::value(x,ref);});return c.status();
}
}
Status visit_military_group_order_acceptance(CodecVisitor& c,MilitaryGroupOrderAcceptance& v){try{return march_codec::fields(c,v);}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Outcome<MilitaryGroupOrderAcceptance> decode_military_group_order_acceptance(const BoundaryValue& input){return movement_detail::guard<MilitaryGroupOrderAcceptance>([&]{try{MilitaryGroupOrderAcceptance v;ActorFoundationJsonVisitor c(input);movement_detail::take(march_codec::fields(c,v));movement_detail::take(c.finish());return v;}catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
Status emit_military_group_order_acceptance_fields(ProjectionWriter& w,const MilitaryGroupOrderAcceptance& v){try{ActorFoundationProjectionArchive c(w);return march_codec::fields(c,const_cast<MilitaryGroupOrderAcceptance&>(v));}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Status emit_military_group_order_acceptance(ProjectionWriter& w,std::string_view name,const MilitaryGroupOrderAcceptance& v){return w.object(name,[&](auto& x){return emit_military_group_order_acceptance_fields(x,v);});}
Outcome<BoundaryValue> encode_military_group_order_acceptance(const MilitaryGroupOrderAcceptance& v){return movement_detail::guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter w(out);movement_detail::take(emit_military_group_order_acceptance(w,"",v));movement_detail::take(w.finish());return out;});}
}

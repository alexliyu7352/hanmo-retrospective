#include "common_projection.hpp"
#include <array>
#include <type_traits>
namespace hanmo::resident {
Status emit_common_owner_fields(ProjectionWriter& w,const Owner& o) {
 if(o.extra)return w.reject("UnsupportedOwnerCompatibility",o.id);
 const char* kind=o.kind==Owner::Kind::actor?"actor":o.kind==Owner::Kind::organization?"organization":o.kind==Owner::Kind::task?"task":o.kind==Owner::Kind::household?"household":nullptr;
 if(!kind)return w.reject("InvalidOwner",o.id);
 w.field("kind",StableId{kind});auto emitted=w.field("id",o.id);if(!emitted.ok)return emitted;return w.order_fields(o.field_order);
}
Status emit_common_owner(ProjectionWriter& w,std::string_view name,const Owner& o) {
 return w.object(name,o.field_order,[&](auto& out){return emit_common_owner_fields(out,o);});
}
Status emit_common_wilderness_location_fields(ProjectionWriter& w,const WildernessLocation& p) {
 w.field("kind",StableId{"Wilderness"});w.field("map_id",p.map_id);const std::array<Integer,2> point{p.point.x,p.point.y};w.sequence("point",point,[](auto& out,Integer n){return out.field("",n);});return w.field("origin",p.origin);
}
Status emit_common_location_fields(ProjectionWriter& w,const Location& v,bool carrier_location_enabled) {
 if(std::holds_alternative<CarriedLocation>(v)&&!carrier_location_enabled)return w.reject("CarrierLocationProfileRequired");
 return std::visit([&](const auto& p)->Status {
  using T=std::decay_t<decltype(p)>;
  if constexpr(std::is_same_v<T,SettlementLocation>){w.field("kind",StableId{"Settlement"});w.field("settlement_id",p.settlement_id);return w.field("place_id",p.place_id);}
  else if constexpr(std::is_same_v<T,WildernessLocation>)return emit_common_wilderness_location_fields(w,p);
  else if constexpr(std::is_same_v<T,FollowingLocation>){w.field("kind",StableId{"Following"});return w.field("party_id",p.party_id);}
  else if constexpr(std::is_same_v<T,InArmyLocation>){w.field("kind",StableId{"InArmy"});return w.field("army_id",p.army_id);}
  else if constexpr(std::is_same_v<T,OnRouteLocation>){w.field("kind",StableId{"OnRoute"});w.field("edge_id",p.edge_id);w.field("progress",p.progress);return w.field("origin",p.origin);}
  else if constexpr(std::is_same_v<T,CarriedLocation>){const char* kind=p.carrier_kind==CarriedLocation::CarrierKind::transport_unit?"transport_unit":p.carrier_kind==CarriedLocation::CarrierKind::vessel?"vessel":p.carrier_kind==CarriedLocation::CarrierKind::fleet?"fleet":nullptr;if(!kind||p.carrier_id.empty())return w.reject("InvalidCarriedLocation");w.field("kind",StableId{"Carried"});w.field("carrier_kind",StableId{kind});return w.field("carrier_id",p.carrier_id);}
  else return w.reject("UnsupportedLocationProjection");
 },v);
}
Status emit_common_location(ProjectionWriter& w,std::string_view name,const Location& v,const FieldOrder& order,bool carrier_location_enabled) {
 return w.object(name,order,[&](auto& out){return emit_common_location_fields(out,v,carrier_location_enabled);});
}
Status emit_common_reservation_fields(ProjectionWriter& w,const ReservationRef& r,bool typed,bool domain_activity_enabled) {
 if(r.kind==ReservationRef::Kind::domain_activity){if(!typed||!domain_activity_enabled)return w.reject("UnsupportedReservationKind",r.id);if(r.id.empty()||r.domain_id.empty()||r.activity_kind.empty())return w.reject("InvalidReservation",r.id);w.field("kind",StableId{"domain_activity"});w.field("id",r.id);w.field("domain_id",r.domain_id);return w.field("activity_kind",r.activity_kind);}
 if(!r.domain_id.empty()||!r.activity_kind.empty())return w.reject("InvalidReservation",r.id);
 // The legacy action representation is a scalar, never a foreign object.
 if(!typed&&r.kind==ReservationRef::Kind::action)return w.reject("ScalarReservationHasNoObjectFields",r.id);
 const char* kind=r.kind==ReservationRef::Kind::action?"action":r.kind==ReservationRef::Kind::contract?"contract":r.kind==ReservationRef::Kind::org_activity?"org_activity":nullptr;
 if(!kind)return w.reject("InvalidReservationKind",r.id);
 w.field("kind",StableId{kind});return w.field("id",r.id);
}
Status emit_common_reservation(ProjectionWriter& w,std::string_view name,const ReservationRef& r,bool typed,bool domain_activity_enabled) {
 if(!typed&&r.kind==ReservationRef::Kind::action){if(!r.domain_id.empty()||!r.activity_kind.empty())return w.reject("InvalidReservation",r.id);return w.field(name,r.id);}
 return w.object(name,[&](auto& out){return emit_common_reservation_fields(out,r,typed,domain_activity_enabled);});
}
namespace {
// 原Lot门与顺序视图共用，不新增可接受字段或临时字段副本。
Status validate_lot_projection(ProjectionWriter& w,const LotView& v,const CommonResourceCodecContext& c) {
 if(v.extra||(v.item_instance_id&&!c.item_instance_refs_enabled)||(v.lot_properties_ref&&!c.lot_property_refs_enabled))return w.reject("CommonExtensionCodecMismatch",v.id);
 if(v.item_instance_id&&v.lot_properties_ref)return w.reject("LotReferenceConflict",v.id);
 if((v.item_instance_id&&v.item_instance_id->empty())||(v.lot_properties_ref&&v.lot_properties_ref->properties_id.empty()))return w.reject("InvalidLotReference",v.id);
 return w.status();
}
ProjectionFieldOrderView lot_projection_order(const LotView& v,const CommonResourceCodecContext& c) {
 ProjectionFieldOrderView order{&v.field_order,{},0};
 if(!v.field_order.empty()) {
  if(c.item_instance_refs_enabled&&std::find(v.field_order.begin(),v.field_order.end(),"item_instance_id")==v.field_order.end())order.appended[order.appended_count++]="item_instance_id";
  if(c.lot_property_refs_enabled&&std::find(v.field_order.begin(),v.field_order.end(),"lot_properties_ref")==v.field_order.end())order.appended[order.appended_count++]="lot_properties_ref";
 }
 return order;
}
// 两个入口唯一字段正文；外壳沿原路径先登记FieldOrder，内嵌入口末尾补同一顺序。
Status emit_lot_values(ProjectionWriter& w,const LotView& v,bool typed,const CommonResourceCodecContext& c) {
 w.field("id",v.id);w.field("item_id",v.item_id);w.field("quantity",v.quantity);
 emit_common_owner(w,"owner",v.owner);
 emit_common_reservation(w,"reserved_by",v.reserved_by,typed,c.domain_activity_reservation_refs_enabled);
 if(c.item_instance_refs_enabled){if(v.item_instance_id)w.field("item_instance_id",*v.item_instance_id);else w.null("item_instance_id");}
 if(c.lot_property_refs_enabled){if(v.lot_properties_ref)w.object("lot_properties_ref",[&](auto& a){return a.field("properties_id",v.lot_properties_ref->properties_id);});else w.null("lot_properties_ref");}
 return w.status();
}
}
// 已打开对象只写原完整Lot；不推断profile或降级typed引用。
Status emit_common_lot_fields(ProjectionWriter& w,const LotView& v,bool typed,const CommonResourceCodecContext& c) {
 auto checked=validate_lot_projection(w,v,c);if(!checked.ok)return checked;
 auto emitted=emit_lot_values(w,v,typed,c);if(!emitted.ok)return emitted;
 return w.order_fields(lot_projection_order(v,c));
}
// 保留原对象外壳、门的先后和扩展字段顺序。
Status emit_common_lot(ProjectionWriter& w,std::string_view name,const LotView& v,bool typed,const CommonResourceCodecContext& c) {
 auto checked=validate_lot_projection(w,v,c);if(!checked.ok)return checked;
 return w.object(name,lot_projection_order(v,c),[&](auto& out){return emit_lot_values(out,v,typed,c);});
}
namespace {
// 仅观察原字段正文在当前对象的声明顺序；不复制值树、字段表或持久FieldOrder。
class OriginalFieldOrderProbe final:public ProjectionWriter {
 const FieldOrder& expected_;std::size_t at_=0,depth_=0;bool equal_=true;
 void name(std::string_view value){if(depth_==0){if(at_>=expected_.size()||expected_[at_]!=value)equal_=false;++at_;}}
 Status scalar(std::string_view key,const Scalar&)override{name(key);return common_success();}
 Status begin_object(std::string_view key,ProjectionFieldOrderView)override{name(key);++depth_;return common_success();}
 Status end_object()override{if(!depth_)return common_failure("CommonFieldOrderProbeDepthMismatch");--depth_;return common_success();}
 Status reorder_object(ProjectionFieldOrderView)override{return common_success();}
 Status begin_array(std::string_view key,std::size_t)override{name(key);++depth_;return common_success();}
 Status end_array()override{return end_object();}
 Status validate_branch(ValidationEmit,const void*)override{return common_failure("CommonFieldOrderProbeBranchUnsupported");}
public:
 explicit OriginalFieldOrderProbe(const FieldOrder& expected):expected_(expected){}
 Status finish()override{return result_.ok&&equal_&&depth_==0&&at_==expected_.size()?common_success():common_failure("CommonFieldOrderNotOriginalCanonical");}
};
template<class Emit>bool equivalent_order(const FieldOrder& a,const FieldOrder& b,Emit emit){
 if(a==b)return true;
 if(!a.empty()&&!b.empty())return false;
 OriginalFieldOrderProbe probe(a.empty()?b:a);auto status=emit(probe);
 return status.ok&&probe.finish().ok;
}
}
bool common_owner_field_order_equivalent(const Owner&a,const Owner&b){return equivalent_order(a.field_order,b.field_order,[&](auto&w){return emit_common_owner_fields(w,a.field_order.empty()?a:b);});}
bool common_lot_field_order_equivalent(const LotView&a,const LotView&b,const CommonResourceCodecContext&c){return equivalent_order(a.field_order,b.field_order,[&](auto&w){return emit_common_lot_fields(w,a.field_order.empty()?a:b,true,c);});}
bool common_inventory_field_order_equivalent(const InventoryView&a,const InventoryView&b,const CommonResourceCodecContext&c){return equivalent_order(a.field_order,b.field_order,[&](auto&w){return emit_common_inventory_frame_fields(w,a.field_order.empty()?a:b,true,c);});}
}

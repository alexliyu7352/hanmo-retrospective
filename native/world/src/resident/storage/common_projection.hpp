// 与Common原JSON writer共用的只读emit；字段顺序和扩展选择由调用方显式传入。
#pragma once
#include "common_codec.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
// Emit into an already-open object. The enclosing archive preserves the original
// Owner/Location FieldOrder. Legacy untyped action reservations remain scalars.
Status emit_common_owner_fields(ProjectionWriter&,const Owner&);
Status emit_common_wilderness_location_fields(ProjectionWriter&,const WildernessLocation&);
Status emit_common_location_fields(ProjectionWriter&,const Location&,bool carrier_location_enabled=false);
Status emit_common_reservation_fields(ProjectionWriter&,const ReservationRef&,bool typed,bool domain_activity_enabled);
Status emit_common_owner(ProjectionWriter&,std::string_view name,const Owner&);
Status emit_common_location(ProjectionWriter&,std::string_view name,const Location&,const FieldOrder& order={},bool carrier_location_enabled=false);
Status emit_common_reservation(ProjectionWriter&,std::string_view name,const ReservationRef&,bool typed,bool domain_activity_enabled);
// 已打开对象使用原Lot字段与引用选择，保留原FieldOrder扩展顺序。
Status emit_common_lot_fields(ProjectionWriter&,const LotView&,bool typed,const CommonResourceCodecContext&);
Status emit_common_lot(ProjectionWriter&,std::string_view name,const LotView&,bool typed,const CommonResourceCodecContext&);
// 冷链只比较原空序的实际默认投影与显式canonical序；不改值或放宽两个显式序。
bool common_owner_field_order_equivalent(const Owner&,const Owner&);
bool common_lot_field_order_equivalent(const LotView&,const LotView&,const CommonResourceCodecContext&);
bool common_inventory_field_order_equivalent(const InventoryView&,const InventoryView&,const CommonResourceCodecContext&);
}

// typed 子型各自在自身 CPP 实例化；基础字段沿原逐字段二进制/JSON/投影出口。
#pragma once
#include "field_event_codec_bridge.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
namespace hanmo::resident::field_event_fields {
template<class C> Status value(C &c, StableId &v) { return c.field("value",v); }
template<class C> Status value(C &c, Integer &v) { return c.field("value",v); }
template<class C> Status value(C &c, bool &v) { return c.field("value",v); }
template<class C> Status value(C &c, PointI &v) { c.field("x",v.x);return c.field("y",v.y); }
template<class C> Status value(C &c, SourceRef &v) { c.field("domain",v.domain);c.field("kind",v.kind);return c.field("id",v.id); }
template<class C> Status value(C &c, FieldMission &v) { return field_event_original_value(c,v); }
template<class C> Status value(C &c, FieldBattleTime &v) { return field_event_original_value(c,v); }
template<class C,class T> Status value(C &c,std::optional<T> &v) { return c.optional("optional",v,[](auto &child,auto &row){return value(child,row);}); }
template<class C,class T> Status field(C &c,const char *name,T &v) { return c.object(name,[&](auto &child){return value(child,v);}); }
} // namespace hanmo::resident::field_event_fields

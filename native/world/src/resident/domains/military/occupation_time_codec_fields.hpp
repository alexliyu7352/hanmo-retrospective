#pragma once
#include "occupation_model.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
namespace hanmo::resident::occupation_time_codec {
// The explicit /2 physical suffix always has these three optional fields.
// All absent is an actual retained legacy row; partial presence is invalid.
template<class C>Status tail(C& c,MilitaryOccupationFact& row){
 std::optional<WorldMoment> at;std::optional<Integer> kind;std::optional<SourceRef> trigger;
 if(row.exact_time){at=row.exact_time->actual_occupation_at;kind=Integer(row.exact_time->producer_kind);trigger=row.exact_time->trigger_source;}
 c.optional("actual_occupation_at",at,[](auto& x,auto& v){return exact_fields::moment(x,v);});
 c.optional("producer_kind",kind,[](auto& x,auto& v){return x.field("value",v);});
 c.optional("trigger_source",trigger,[](auto& x,auto& v){x.field("domain",v.domain);x.field("kind",v.kind);return x.field("id",v.id);});
 if(!c.status().ok)return c.status();if(bool(at)!=bool(kind)||bool(at)!=bool(trigger))return c.reject("MilitaryOccupationExactTailIncomplete",row.id);
 if(kind&&(*kind<0||*kind>2))return c.reject("MilitaryOccupationProducerKindMismatch",row.id);
 if(c.reading()){row.exact_time.reset();if(at)row.exact_time=MilitaryOccupationTime2{std::move(*at),MilitaryOccupationProducerKind(*kind),std::move(*trigger)};}
 return c.status();
}
}

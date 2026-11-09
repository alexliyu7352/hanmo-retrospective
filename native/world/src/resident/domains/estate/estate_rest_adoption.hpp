// Embedded in the original Growth period. This is an adopted input, not a new
// estate balance, period, or Common fact. Growth owns the action/ordinal
// binding.
#pragma once
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct EstateRestAdoptionBasis {
 Integer source_schema=1;
 StableId estate_id;
 ScenarioOriginRef initial_origin;
 WorldMoment adopted_at;
 // The exact already-applied Common prefix at the real consumption boundary.
 // Later facts at the same Moment cannot change this historical input.
 Integer common_fact_count=0;
 std::optional<SourceRef> last_common_fact;
};
// Embedded owners use this exact field list with their own existing archive.
// Nested ScenarioOriginRef/WorldMoment/SourceRef retain their original codecs.
template<class Archive,class Value>
void estate_rest_adoption_fields(Archive& archive,Value& value){
 archive("source_schema",value.source_schema);archive("estate_id",value.estate_id);
 archive("initial_origin",value.initial_origin);archive("adopted_at",value.adopted_at);
 archive("common_fact_count",value.common_fact_count);archive("last_common_fact",value.last_common_fact);
}
Status validate_estate_rest_adoption_shape(const EstateRestAdoptionBasis&);
class ProjectionWriter;class CodecVisitor;
Status emit_estate_rest_adoption(ProjectionWriter&,const StableId& name,const EstateRestAdoptionBasis&);
BoundaryValue encode_estate_rest_adoption(const EstateRestAdoptionBasis&);
Outcome<EstateRestAdoptionBasis> decode_estate_rest_adoption(const BoundaryValue&);
Status visit_estate_rest_adoption(CodecVisitor&,EstateRestAdoptionBasis&);
}

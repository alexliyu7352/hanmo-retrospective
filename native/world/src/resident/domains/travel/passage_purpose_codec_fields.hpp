// The original PASSAGE-2 purpose context, also used by selected S6 sources.
#pragma once
#include "passage_model.hpp"
namespace hanmo::resident::passage_purpose_codec_fields {
template<class Archive>Status fields(Archive& c,PassagePurposeContext& value){
 StableId kind=std::holds_alternative<OrdinaryPassageContext>(value)?"ordinary":"safe_exit";c.field("kind",kind);
 if(!c.status().ok)return c.status();if(kind!="ordinary"&&kind!="safe_exit")return c.reject("UnknownPassagePurposeContext");
 if(c.reading())value=kind=="ordinary"?PassagePurposeContext{OrdinaryPassageContext{}}:PassagePurposeContext{SafeExitPassageContext{}};
 if(auto* v=std::get_if<SafeExitPassageContext>(&value)){c.field("binding_id",v->binding_id);c.sequence("line_refs",v->line_refs,[](auto& a,auto& id){return a.field("value",id);});c.field("path_id",v->path_id);c.field("next_boundary_ordinal",v->next_boundary_ordinal);}
 if(c.status().ok){const auto valid=validate_passage_purpose_context_shape(value);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();
}
}

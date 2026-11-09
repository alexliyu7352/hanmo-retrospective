// Canonical original age-basis references. No copied birthday or current age.
#pragma once
#include "birth_fact_ref.hpp"
#include "resident/storage/common_store.hpp"
#include <variant>
#include <string_view>
namespace hanmo::resident {
struct DeclaredAgeBasisRef {enum class Kind{actor_start,creation_registration,population_registration};Kind kind=Kind::actor_start;StableId actor_id,source_ref;};
using LifeAgeBasisRef=std::variant<BirthFactRef,DeclaredAgeBasisRef>;
template<class C>Status life_age_basis_fields(C& c,LifeAgeBasisRef& value){
    StableId kind=std::holds_alternative<BirthFactRef>(value)?"birth":"declared_age";c.field("kind",kind);
    if(kind!="birth"&&kind!="declared_age")return c.reject("InvalidLifeAgeBasisKind");
    if(c.reading())value=kind=="birth"?LifeAgeBasisRef{BirthFactRef{}}:LifeAgeBasisRef{DeclaredAgeBasisRef{}};
    if(auto* b=std::get_if<BirthFactRef>(&value)){c.field("birth_fact_id",b->birth_fact_id);return b->birth_fact_id.empty()?c.reject("InvalidLifeAgeBirthRef"):c.status();}
    auto& d=std::get<DeclaredAgeBasisRef>(value);Integer tag=static_cast<Integer>(d.kind);c.field("source_kind",tag);if(tag<0||tag>2)return c.reject("InvalidDeclaredAgeSourceKind");if(c.reading())d.kind=static_cast<DeclaredAgeBasisRef::Kind>(tag);c.field("actor_id",d.actor_id);c.field("source_ref",d.source_ref);return d.actor_id.empty()||d.source_ref.empty()?c.reject("InvalidDeclaredAgeBasisRef"):c.status();
}

bool same_life_age_basis_ref(const LifeAgeBasisRef&,const LifeAgeBasisRef&);
class CodecVisitor;class ProjectionWriter;
Status visit_life_age_basis_ref(CodecVisitor&,LifeAgeBasisRef&);
Status emit_life_age_basis_ref(ProjectionWriter&,std::string_view,const LifeAgeBasisRef&);
Outcome<LifeAgeBasisRef> decode_life_age_basis_ref(const BoundaryValue&);
struct FamilyDecisionAgeSources4 {LifeAgeBasisRef predecessor,successor;};
template<class C>Status family_decision_age_sources4_fields(C& c,FamilyDecisionAgeSources4& value){c.object("predecessor",[&](C& v){return life_age_basis_fields(v,value.predecessor);});return c.object("successor",[&](C& v){return life_age_basis_fields(v,value.successor);});}
Status visit_family_decision_age_sources4(CodecVisitor&,FamilyDecisionAgeSources4&);
Status emit_family_decision_age_sources4(ProjectionWriter&,std::string_view,const FamilyDecisionAgeSources4&);
Outcome<FamilyDecisionAgeSources4> decode_family_decision_age_sources4(const BoundaryValue&);
}

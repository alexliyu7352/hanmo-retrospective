// Original Birth4 source before downstream placement/population/care effects.
// The full Birth validator independently requires those effects at publication.
#pragma once
#include "family_birth_identity.hpp"
#include "family_continuation_people4.hpp"
namespace hanmo::resident {
struct BornCharacterSourceContext {
 const CommonStore& common;const CommonCatalog& common_catalog;
 const FamilyCatalog& family_catalog;const FamilyLineageCatalog& lineage;
 const InteractionCatalog& interactions;const BirthArrangementCatalog& catalog;
 const FamilyBirthState& births;const BirthArrangementAccess& access;
};
struct BornCharacterSourceView {
 const FamilyBirthRecord* birth=nullptr;const BirthReply* accepted=nullptr;
 const IdentityPrototype* prototype=nullptr;const FactRecord* original_fact=nullptr;
 WorldMoment occurred_at;
};
// Pre-allocation read for S1's actual due placement query. No child, BirthFact,
// placement, provision or care result is needed to establish this authority.
// The accepted row is borrowed only for the current candidate read lease.
struct BirthPlacementArrangementView {
 const BirthReply* accepted=nullptr;const FactRecord* accepted_fact=nullptr;
 ScheduledBirthSource scheduled;
};
Outcome<BirthPlacementArrangementView> read_birth_placement_arrangement_at_due(
 const BornCharacterSourceContext&,const StableId& delivery_actor_id,
 const StableId& accepted_id,const ReadStamp&,const ReadStampAuthority&,
 const DomainReadVersionSource&);
// These checks never query a downstream location, Condition, population,
// provision or care result. No circular result can establish its own cause.
Status validate_born_character_source_structure(const BornCharacterSourceContext&);
Outcome<BornCharacterSourceView> read_born_character_source(const BornCharacterSourceContext&,const BirthFactRef&);
Outcome<InnateParentSource> read_born_original_innate_parent(const BornCharacterSourceContext&,const StableId& actor,const StableId& family_id);
class BornCharacterSourceRead {
public:virtual ~BornCharacterSourceRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Outcome<BornCharacterSourceView> read_original_birth(const BirthFactRef&)const=0;
};
class NativeBornCharacterSourceRead final:public BornCharacterSourceRead {
 const BornCharacterSourceContext& context_;
public:explicit NativeBornCharacterSourceRead(const BornCharacterSourceContext& c):context_(c){}
 bool bound_to(const CommonStore& c)const noexcept override{return &context_.common==&c;}
 Outcome<BornCharacterSourceView> read_original_birth(const BirthFactRef& ref)const override{return read_born_character_source(context_,ref);}
};
}

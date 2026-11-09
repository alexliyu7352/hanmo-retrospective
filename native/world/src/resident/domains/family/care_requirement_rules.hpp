#pragma once
#include "care_requirement_contract.hpp"
namespace hanmo::resident {
// Borrowed catalog rules are selected once by the original author pipeline.
// They never accept a caller-supplied rule or substitute the birth agreement's
// preferred caregiver for an actual accepted care/handover source.
struct CareRequirementCatalog {
 CatalogIdentity identity;Integer selected_version=0;
 StableId birth_requirement_rule_id;OrderedTable<CareRequirementRule> rules;
};
Status validate_care_requirement_catalog(const CareRequirementCatalog&);
Outcome<CareRequirementCatalog> decode_care_requirement_catalog(const BoundaryValue& definitions,const BoundaryValue& selected_config,const CatalogIdentity&);
// The complete module selector owns this finite version. The author fields
// stay unchanged; neither their presence nor an empty section enables FH.
// The three-argument legacy entry remains strictly version 3.
Outcome<CareRequirementCatalog> decode_care_requirement_catalog(const BoundaryValue& definitions,const BoundaryValue& selected_config,const CatalogIdentity&,Integer trusted_selected_version);
class NativeCareRequirementRuleRead final:public CareRequirementRuleRead {
 const CommonStore& common_;const CareRequirementCatalog& catalog_;const BornCharacterSourceRead& births_;
public:
 NativeCareRequirementRuleRead(const CommonStore& common,const CareRequirementCatalog& catalog,const BornCharacterSourceRead& births):common_(common),catalog_(catalog),births_(births){}
 bool bound_to(const CommonStore& c)const noexcept override{return &common_==&c&&births_.bound_to(c);}
 Integer selected_care_version()const noexcept override{return catalog_.selected_version;}
 Outcome<const CareRequirementRule*> read_birth_care_rule(const BornCharacterSourceView&)const override;
 Outcome<const CareRequirementRule*> read_care_requirement_rule(const ActorFoundationRuleRef&)const override;
};
}

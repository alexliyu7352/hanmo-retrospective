// 接管后税政独立保存；政策修订不改变地区原税期、纳税主体或既存债权。
#pragma once
#include "politics_occupation_fiscal_contract.hpp"
#include "politics_takeover_rules.hpp"
#include "resident/domains/politics/politics_fiscal.hpp"
namespace hanmo::resident {
struct OccupationFiscalCatalog { bool enabled=false;Integer version=1; };
struct OccupationFiscalPolicyRecord {
    StableId id,registration_fact_id,grant_id;
    ActingAuthority authority;
    FiscalPolicySnapshot policy;
};
struct OccupationFiscalState { Integer version=1;OrderedTable<OccupationFiscalPolicyRecord> policies; };
struct PoliticalSetOccupationTaxCommand {
    ActingAuthority authority;
    StableId jurisdiction_id,registration_fact_id,public_inventory_id;
    Integer expected_jurisdiction_revision=0,expected_policy_revision=0;
    FiscalTaxMode mode=FiscalTaxMode::normal;
};
struct OccupationFiscalContext {
    PoliticalTakeoverContext takeover;
    const PoliticsState& politics;
    const OccupationFiscalCatalog& catalog;
    const OccupationFiscalState& state;
    const PoliticalFiscalState& legacy;
};
Status validate_occupation_fiscal(const OccupationFiscalContext&);
Status preview_political_set_occupation_tax(const OccupationFiscalContext&,const ActivityRead&,const StableId& actor,const PoliticalSetOccupationTaxCommand&);
Outcome<PoliticalFiscalEffects> apply_political_set_occupation_tax_in_transaction(CommonStore&,OccupationFiscalState&,const OccupationFiscalContext&,const ActivityRead&,const StableId& actor,const PoliticalSetOccupationTaxCommand&);
// 老命令在发生过真实接管的辖区退出；原政权收复也必须新签本段政策。
Status require_legacy_fiscal_before_occupation(const PoliticalTakeoverContext&,const StableId& jurisdiction_id);
class OccupationFiscalAccess final:public OccupationFiscalPolicyRead {
public:
    explicit OccupationFiscalAccess(OccupationFiscalContext context):context_(context){}
    Outcome<std::optional<OccupationFiscalPolicySource>> occupation_fiscal_policy_for_due_tick(const StableId&,Integer)const override;
    Outcome<std::optional<OccupationFiscalPolicySource>> occupation_fiscal_policy_for_assessment_source(const StableId&,Integer,const StableId&)const override;
private:
    OccupationFiscalContext context_;
    Outcome<std::optional<OccupationFiscalPolicySource>> select(const StableId&,Integer,const StableId*)const;
};
BoundaryValue encode_occupation_fiscal_state(const OccupationFiscalState&);
Outcome<OccupationFiscalState> decode_occupation_fiscal_state(const BoundaryValue&);
Status visit_occupation_fiscal_checkpoint(CodecVisitor&,OccupationFiscalState&);
BoundaryValue encode_occupation_fiscal_catalog(const OccupationFiscalCatalog&);
Outcome<OccupationFiscalCatalog> decode_occupation_fiscal_catalog(const BoundaryValue&);
BoundaryValue encode_political_set_occupation_tax(const PoliticalSetOccupationTaxCommand&);
Outcome<PoliticalSetOccupationTaxCommand> decode_political_set_occupation_tax(const BoundaryValue&);
Outcome<BoundaryValue> query_occupation_fiscal_entries(const OccupationFiscalContext&,const ActivityRead&,const StableId& actor,const StableId& site);
bool occupation_fiscal_grant_declares_resource(const CommonCatalog&,const OrganizationCatalog&,const PoliticsCatalog&,const MilitaryCatalog&,const PoliticalTakeoverCatalog&,const OccupationFiscalCatalog&,const StableId& grant_id,const ResourceKey&);
}

// 报价只冻结同一候选与具名新税政Grant，提交时重新核控制、任期、场所和公库。
#pragma once
#include "politics_occupation_fiscal.hpp"
namespace hanmo::resident {
struct OccupationFiscalQuote {
    WorldIdentity source_identity;
    StableId actor_id,grant_id;
    PoliticalSetOccupationTaxCommand command;
    Integer duration=0;
};
Outcome<OccupationFiscalQuote> prepare_occupation_fiscal_command(const OccupationFiscalContext&,const ActivityRead&,const StableId& actor,const PoliticalSetOccupationTaxCommand&);
Outcome<PoliticalFiscalEffects> commit_occupation_fiscal_command_in_transaction(CommonStore&,OccupationFiscalState&,const OccupationFiscalContext&,const ActivityRead&,const StableId& actor,const PoliticalSetOccupationTaxCommand&,const OccupationFiscalQuote&);
BoundaryValue encode_occupation_fiscal_quote(const OccupationFiscalQuote&);
}

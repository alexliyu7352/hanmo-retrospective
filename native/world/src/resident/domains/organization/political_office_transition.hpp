// The actual politics arrival/dismissal fact is upstream of the one Office writer.
#pragma once
#include "office_lifecycle.hpp"
#include "resident/domains/politics/politics_model.hpp"
namespace hanmo::resident {
class PoliticalOfficeTransitionRead final:public OfficeTransitionSourceRead {
 const CommonStore& common_;const PoliticsState& politics_;const OrganizationState& organization_;const OrganizationCatalog& catalog_;
 const OrderedTable<VerifiedOfficeTransition>& sources_;const OrganizationAuthorityClock& clock_;
public:
 PoliticalOfficeTransitionRead(const CommonStore& c,const PoliticsState& p,const OrganizationState& o,const OrganizationCatalog& catalog,const OrderedTable<VerifiedOfficeTransition>& sources,const OrganizationAuthorityClock& clock):common_(c),politics_(p),organization_(o),catalog_(catalog),sources_(sources),clock_(clock){}
 Outcome<VerifiedOfficeTransition> read_office_transition(const SourceRef&,const ReadStamp&)const override;
};
// Called only after the original politics preview and actual immutable fact append succeeded.
// The registry is OrganizationRuntime.office_transition_sources, never another copy.
Outcome<OfficeChangeResult> apply_political_office_transition_after_fact(CommonStore&,const PoliticsState&,OrganizationState&,const OrganizationCatalog&,OrderedTable<VerifiedOfficeTransition>&,const StableId& actual_fact_id,const OrganizationAuthorityClock&,const ReadStamp&);
}

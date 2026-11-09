// 显式撤职来源/2：实际调用者、单一Basis1与原Office旧任期；旧/1自行保留完整窄协议。
#pragma once
#include "resident/domains/politics/politics_time2.hpp"
#include "resident/domains/politics/politics_territory_contract.hpp"
#include "resident/domains/actor_foundation/character_presence_v2.hpp"
namespace hanmo::resident {
struct PoliticalDismissalRecord2 {
 SourceRef dismissal_source,office_change_source;
 AuthorityBasisRef authority_basis_ref;
 Integer jurisdiction_revision=0;
};
struct PoliticalDismissalSources2 {Integer version=2;OrderedTable<PoliticalDismissalRecord2> dismissals;};
// 真实人物Origin/Death4和原成员史；先由调用方核政治payload的M与真实Common边界。
class NativePoliticalDismissalHistoryRead {
 const CommonStore& common_;const OrganizationState& organizations_;const OrganizationCatalog& catalog_;
 const LifeAgeSources& ages_;const AuthoredActorFoundationState& foundation_;const ActorFoundationCatalog& foundation_catalog_;
 const FamilyDeathState& deaths_;const FamilyLifeCauseSources4& death_sources_;const OrganizationAuthorityClock& clock_;
 const FamilyCatalog* family_catalog_=nullptr;const CommonCatalog* shared_=nullptr;
public:
 NativePoliticalDismissalHistoryRead(const CommonStore& c,const OrganizationState& o,const OrganizationCatalog& oc,const LifeAgeSources& ages,const AuthoredActorFoundationState& f,const ActorFoundationCatalog& fc,const FamilyDeathState& d,const FamilyLifeCauseSources4& ds,const OrganizationAuthorityClock& clock,const FamilyCatalog* family_catalog=nullptr,const CommonCatalog* shared=nullptr):common_(c),organizations_(o),catalog_(oc),ages_(ages),foundation_(f),foundation_catalog_(fc),deaths_(d),death_sources_(ds),clock_(clock),family_catalog_(family_catalog),shared_(shared){}
 // 同一个源候选及真实时钟；实际born consumer仍借唯一LifeAgeSources.born_sources。
 bool bound_to(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const OrganizationAuthorityClock&)const noexcept;
 Status validate_signer_before(const ActingAuthority&,const SourceRef&,const WorldMoment&)const;
};
struct PoliticalDismissal2Context {
 const CommonStore& common;const PoliticsState& politics;const PoliticsCatalog& politics_catalog;
 const OrganizationState& organizations;const OrganizationCatalog& organization_catalog;
 const LocalEconomyState& economy;const ActivityRead& activity;const PoliticalDismissalSources2& state;
 const PoliticalOrganizationRuntimeWriteContext& runtime;
 const PoliticalTerritoryRead& territories;const NativePoliticalDismissalHistoryRead& history;
};
struct PoliticalDismissalQuote2 {WorldIdentity identity;WorldMoment quoted_at;PoliticalDismissCommand command;StableId actor_id,removed_actor_id;Integer next_instance_id=0;};
Outcome<Integer> political_dismissal2_selected_version(const std::vector<std::pair<StableId,StableId>>&);
Status validate_political_dismissal2(const PoliticalDismissal2Context&);
Outcome<PoliticalDismissalQuote2> preview_political_dismissal2(const PoliticalDismissal2Context&,const StableId&,const PoliticalDismissCommand&);
// 仅整World私有候选可用；任何失败须丢弃Common/Politics/Organization/Runtime/本段全部候选。
Outcome<PoliticsEffects> apply_political_dismissal2_in_candidate(CommonStore&,PoliticsState&,OrganizationState&,PoliticalDismissalSources2&,const PoliticalDismissal2Context&,const StableId&,const PoliticalDismissCommand&,const PoliticalDismissalQuote2&);
Status visit_political_dismissal2(CodecVisitor&,PoliticalDismissalSources2&);
Status emit_political_dismissal2_projection(ProjectionWriter&,std::string_view,const PoliticalDismissalSources2&);
Outcome<BoundaryValue> encode_political_dismissal2(const PoliticalDismissalSources2&);
Outcome<PoliticalDismissalSources2> decode_political_dismissal2(const BoundaryValue&);
}

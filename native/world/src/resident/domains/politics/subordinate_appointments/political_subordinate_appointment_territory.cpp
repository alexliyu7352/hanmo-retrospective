#include "resident/domains/military/military_model.hpp"
// 首府办理只读实际辖区与同次军事资产应用；不能用今天收复后的Owner复活原补任规程。
#include "political_subordinate_appointment_control.hpp"
#include "resident/domains/politics/takeover/politics_takeover_state.hpp"
#include "resident/domains/military/occupation_rules.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <limits>
#include <map>
#include <set>
namespace hanmo::resident {namespace {
Status jurisdiction(const PoliticalSubordinateTerritoryContext& x,const PoliticalSubordinateAppointmentPolicy& p,Integer tick,const StableId& boundary){
 const auto& common=x.common;const auto* initial=x.catalog.jurisdictions.find(p.jurisdiction_id);const auto* current=x.politics.jurisdictions.find(p.jurisdiction_id);
 if(!x.catalog.enabled||!initial||!current||initial->id!=p.jurisdiction_id||initial->revision!=0||initial->political_organization_id!=initial->controlling_organization_id||current->office_id!=initial->office_id||current->seat_place_id!=initial->seat_place_id||current->political_organization_id!=initial->political_organization_id)return common_failure("PoliticalSubordinateAppointmentOriginalSeatRequired",p.id);
 const Integer through=common.tick();if(tick<0||tick>common.tick()||(boundary.empty()&&tick!=common.tick()))return common_failure("PoliticalSubordinateAppointmentSeatClockMismatch");auto cutoff=boundary.empty()?std::optional<Integer>{common.state().next_instance_id}:(common.state().facts.find(boundary)?political_subordinate_appointment_fact_sequence(common,boundary):std::nullopt);if(!cutoff)return common_failure("PoliticalSubordinateAppointmentSeatBoundaryRequired",boundary);
 std::map<StableId,std::size_t> positions;std::size_t index=0;for(const auto& id:common.state().facts.order())positions[id]=index++;if(!boundary.empty()&&!positions.count(boundary))return common_failure("PoliticalSubordinateAppointmentSeatBoundaryRequired",boundary);
 auto before=[&](const StableId& a,const StableId& b){return positions.count(a)&&positions.count(b)&&positions.at(a)<positions.at(b);};Integer revision=0,at_revision=0,last_number=0,last_tick=-1;StableId owner=initial->controlling_organization_id,previous;std::set<StableId> applied;
 if(x.occupations){const auto& t=*x.occupations;if(!t.catalog.enabled||t.catalog.version!=1||t.state.version!=1)return common_failure("PoliticalSubordinateAppointmentControlHistoryUnavailable");
  for(const auto& row:t.state.registrations.rows()){const auto& r=row.second;const auto* rule=t.catalog.rules.find(r.rule_id);if(!rule)return common_failure("PoliticalSubordinateAppointmentControlHistoryMismatch",row.first);if(rule->jurisdiction_id!=p.jurisdiction_id)continue;auto n=organization_instance_sequence(row.first,"political_takeover_pending");const auto* h=common.state().facts.find(row.first);
   if(row.first!=r.id||!n||*n<=last_number||*n>=common.state().next_instance_id||r.captured_next_instance_id!=*n||!h||h->id!=row.first||h->kind!=political_takeover_pending_fact_kind||h->domain!="politics"||r.tick<last_tick||r.tick<0||r.tick>through||r.former_organization_id!=owner||r.source_identity.world_id!=common.state().identity.world_id||(!previous.empty()&&!before(previous,r.id)))return common_failure("PoliticalSubordinateAppointmentControlHistoryMismatch",r.id);
   const MilitaryOccupationFact* military=nullptr;for(const auto& f:t.military.occupation_facts)if(f.id==r.occupation_fact_id){if(military)return common_failure("PoliticalSubordinateAppointmentControlHistoryMismatch",r.id);military=&f;}auto m=organization_instance_sequence(r.occupation_fact_id,"military");
   if(!military||!m||*m==std::numeric_limits<Integer>::max()||*m+1!=*n||!military->assets_applied.value_or(false)||!military->asset_manifest||military->tick!=r.tick||military->from_organization_id!=r.former_organization_id||military->to_organization_id!=r.successor_organization_id||military->garrison_id!=rule->garrison_id||military->stronghold_id!=rule->stronghold_id||military->public_wallet_ids!=r.public_wallet_ids||military->public_inventory_ids!=r.public_inventory_ids||!before(military->id,r.id)||!applied.insert(military->id).second)return common_failure("PoliticalSubordinateAppointmentControlHistoryMismatch",r.id);
   auto valid=validate_applied_occupation_manifest(common,t.military,t.military_catalog,t.military_access,military->id);if(!valid.ok)return valid;const bool earlier=*n<*cutoff;if((earlier&&r.tick>tick)||(!boundary.empty()&&(earlier!=before(r.id,boundary)||(!earlier&&r.tick<tick))))return common_failure("PoliticalSubordinateAppointmentControlOrderMismatch",r.id);
   if(revision==std::numeric_limits<Integer>::max())return common_failure("PoliticalSubordinateAppointmentControlHistoryMismatch",r.id);++revision;if(earlier)++at_revision;owner=r.successor_organization_id;previous=r.id;last_number=*n;last_tick=r.tick;
  }
  // 该首府真实资产应用不能因遗漏政治登记而被当成从未占领；其他辖区仍由完整原账核。
  for(const auto& f:t.military.occupation_facts)if(f.assets_applied.value_or(false))for(const auto& row:t.catalog.rules.rows())if(row.second.jurisdiction_id==p.jurisdiction_id&&row.second.garrison_id==f.garrison_id&&row.second.stronghold_id==f.stronghold_id&&!applied.count(f.id))return common_failure("PoliticalSubordinateAppointmentControlRegistrationRequired",f.id);
 }
 if(current->revision!=revision||current->controlling_organization_id!=owner)return common_failure("PoliticalSubordinateAppointmentControlHistoryUnavailable",p.id);
 return at_revision==0?common_success():common_failure("PoliticalSubordinateAppointmentPolicyContinuityRequired",p.id);
}
}
// 补任对象的辖区及任官首府必须分别保有原控制，不能拿一处合法掩盖另一处易手。
Status validate_political_subordinate_appointment_seats(const PoliticalSubordinateTerritoryContext& x,const PoliticalSubordinateAppointmentPolicy& p,Integer tick,const StableId& boundary){auto target=jurisdiction(x,p,tick,boundary);if(!target.ok)return target;const auto* head=x.heads.policies.find(p.head_succession_policy_id);if(!head)return common_failure("PoliticalSubordinateAppointmentOriginalSeatRequired",p.id);auto capital=p;capital.jurisdiction_id=head->jurisdiction_id;return jurisdiction(x,capital,tick,boundary);}
}

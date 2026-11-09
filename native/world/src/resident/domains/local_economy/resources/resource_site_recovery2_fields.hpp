#pragma once
#include "resource_site_recovery2.hpp"
#include "resource_site_recovery_fields.hpp"
namespace hanmo::resident {


}
namespace hanmo::resident::resource_recovery2_fields {
using namespace exact_fields;
using resource_recovery_fields::begin;
template<class C>Status state(C&c,ResourceSiteRecoveryState2&v){begin(c);
 required_ref(c,"site_id",v.site_id);
 c.field("remaining_units",v.remaining_units);
 c.field("last_recovery_tick",v.last_recovery_tick);
 c.field("revision",v.revision);
 c.object("recovery_rule_ref",[&](auto&x){return resource_recovery_fields::rule_ref(x,v.recovery_rule_ref);});
 c.object("fractional_remainder",[&](auto&x){return fraction(x,v.fractional_remainder);});
 if(c.status().ok){
 auto one=take(normalize_fraction("1","1"));
 if(v.remaining_units<0||v.last_recovery_tick<0||v.revision<0||take(compare_fractions(v.fractional_remainder,CanonicalFraction{}))<0||take(compare_fractions(v.fractional_remainder,one))>=0)return c.reject("InvalidResourceRecoveryState2",v.site_id);
 }return c.status();}
template<class C>Status receipt(C&c,ResourceRecoveryReceipt2&v){begin(c);
 required_ref(c,"site_id",v.site_id);
 c.field("from_tick",v.from_tick);
 c.field("to_tick",v.to_tick);
 c.object("rule_ref",[&](auto&x){return resource_recovery_fields::rule_ref(x,v.rule_ref);});
 c.field("season_id",v.season_id);
 c.sequence("intensive_receipt_refs",v.intensive_receipt_refs,[](auto&x,auto&r){begin(x);return source(x,r);});
 c.field("standing_before",v.standing_before);
 c.field("standing_after",v.standing_after);
 c.field("recovered_units",v.recovered_units);
 c.field("discarded_whole_units",v.discarded_whole_units);
 c.object("effective_rate",[&](auto&x){return fraction(x,v.effective_rate);});
 c.object("remainder_before",[&](auto&x){return fraction(x,v.remainder_before);});
 c.object("remainder_after",[&](auto&x){return fraction(x,v.remainder_after);});
 c.sequence("state_source_refs",v.state_source_refs,[](auto&x,auto&r){return resource_recovery_fields::original(x,r);});
 if(c.status().ok){
 auto one=take(normalize_fraction("1","1"));
 if(v.from_tick<0||v.from_tick==std::numeric_limits<Integer>::max()||v.to_tick!=v.from_tick+1||v.standing_before<0||v.standing_after<v.standing_before||v.recovered_units!=v.standing_after-v.standing_before||v.discarded_whole_units<0||take(compare_fractions(v.remainder_before,one))>=0||take(compare_fractions(v.remainder_after,one))>=0)return c.reject("InvalidResourceRecoveryReceipt2",v.site_id);
 for(std::size_t i=0;i<v.intensive_receipt_refs.size();++i)for(std::size_t j=0;j<i;++j)if(same_source_ref(v.intensive_receipt_refs[i],v.intensive_receipt_refs[j]))return c.reject("DuplicateGatherRecoveryReceipt",v.intensive_receipt_refs[i].id);
 for(std::size_t i=0;i<v.state_source_refs.size();++i)for(std::size_t j=0;j<i;++j)if(same_source_ref(v.state_source_refs[i].source_ref,v.state_source_refs[j].source_ref))return c.reject("DuplicateResourceRecoveryStateSource",v.state_source_refs[i].source_ref.id);
 }return c.status();}
}

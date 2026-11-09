// 军籍适配器严格冷字段；单独可选域不改变旧MilitaryState的二进制布局。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "population_military.hpp"
#include "population_military_projection.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/domains/organization/authority_basis_codec_fields.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/kernel/world_timeline_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <limits>
#include <set>
#include <type_traits>
namespace hanmo::resident {
namespace population_codec {
StableId version_path(Integer observed) {
  return "population_military/observed_version/" + std::to_string(observed) +
         "/expected_version/4";
}
template <class C, class T> Status proof_binding_fields(C &c, T &value) {
  if constexpr (std::is_same_v<T, WorldIdentity>)
    return authority_basis_codec_fields::world(c, value);
  else if constexpr (std::is_same_v<T, OfficeAuthoritySource>)
    return authority_basis_codec_fields::office_source(c, value);
  else
    return authority_basis_codec_fields::grant_source(c, value);
}
template <class T>
constexpr bool proof_binding_value = std::is_same_v<T, WorldIdentity> ||
                                     std::is_same_v<T, OfficeAuthoritySource> ||
                                     std::is_same_v<T, GrantAuthoritySource>;
template<class T> struct Vector:std::false_type{};
template<class T> struct Vector<std::vector<T>>:std::true_type{using Value=T;};
template<class T> struct Table:std::false_type{};
template<class T> struct Table<OrderedTable<T>>:std::true_type{using Value=T;};
template<class T> struct Enum;
template<> struct Enum<PopulationSoldierStatus>{static auto rows(){using E=PopulationSoldierStatus;return std::vector<std::pair<E,StableId>>{{E::fit,"fit"},{E::wounded,"wounded"},{E::dead,"dead"},{E::captured,"captured"},{E::deserted,"deserted"},{E::released,"released"}};}};
template<> struct Enum<ScopeRef::Kind>{static auto rows(){using E=ScopeRef::Kind;return std::vector<std::pair<E,StableId>>{{E::site,"site"},{E::region,"region"},{E::force,"force"},{E::account,"account"},{E::activity,"activity"}};}};
template<> struct Enum<ResourceKey::Kind>{static auto rows(){using E=ResourceKey::Kind;return std::vector<std::pair<E,StableId>>{{E::actor,"actor"},{E::wallet,"wallet"},{E::inventory,"inventory"},{E::action,"action"},{E::party,"party"},{E::army,"army"}};}};
// 严格遍历PopulationServiceConsent的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,PopulationServiceConsent>,int> =0> void record(A& a,V& v){a("id",v.id);a("cohort_id",v.cohort_id);a("organization_id",v.organization_id);a("garrison_id",v.garrison_id);a("from_tick",v.from_tick);a("through_tick",v.through_tick);a("max_active_count",v.max_active_count);}
// 严格遍历PopulationMilitarySource的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,PopulationMilitarySource>,int> =0> void record(A& a,V& v){a("id",v.id);a("organization_id",v.organization_id);a("organization_office_id",v.organization_office_id);a("military_office_id",v.military_office_id);a("garrison_id",v.garrison_id);a("unit_id",v.unit_id);a("reserve_cohort_id",v.reserve_cohort_id);a("serving_cohort_id",v.serving_cohort_id);a("wounded_cohort_id",v.wounded_cohort_id);a("captive_cohort_id",v.captive_cohort_id);a("displaced_cohort_id",v.displaced_cohort_id);a("home_site_id",v.home_site_id);a("household_group_id",v.household_group_id);a("service_consent_source_id",v.service_consent_source_id);a("wage_obligation_ids",v.wage_obligation_ids);a("capacity",v.capacity);a("initial_training",v.initial_training);a("initial_fatigue",v.initial_fatigue);a("initial_morale",v.initial_morale);}
// 接收政策严格保留独立服役许可和新工资来源。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,PopulationMilitaryIntegrationPolicy>,int> =0> void record(A& a,V& v){a("id",v.id);a("source_id",v.source_id);a("receiving_organization_id",v.receiving_organization_id);a("receiving_garrison_id",v.receiving_garrison_id);a("service_consent_source_id",v.service_consent_source_id);a("wage_obligation_ids",v.wage_obligation_ids);a.optional("ration_obligation_ids",v.ration_obligation_ids);a.optional("target_stronghold_id",v.target_stronghold_id);}
// 严格遍历PopulationMilitaryCatalog的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,PopulationMilitaryCatalog>,int> =0> void record(A& a,V& v){a("enabled",v.enabled);a("consents",v.consents);a("sources",v.sources);a.optional("integration_policies",v.integration_policies);a.optional("selected_version",v.selected_version);}
// 严格遍历ActingAuthority的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,ActingAuthority>,int> =0> void record(A& a,V& v){a("organization_id",v.organization_id);a("office_id",v.office_id);a("authority_epoch",v.authority_epoch);a("actor_id",v.actor_id);}
// 严格遍历ScopeRef的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,ScopeRef>,int> =0> void record(A& a,V& v){a("kind",v.kind);a("id",v.id);}
// 严格遍历ResourceKey的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,ResourceKey>,int> =0> void record(A& a,V& v){a("kind",v.kind);a("id",v.id);}
// 严格遍历ReadVersion的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,ReadVersion>,int> =0> void record(A& a,V& v){a("resource",v.resource);a("version",v.version);}
// 严格遍历AuthorityProof的全部持久字段，缺失或多余字段均拒绝。
template <class A, class V,
          std::enable_if_t<
              std::is_same_v<std::remove_const_t<V>, AuthorityProof>, int> = 0>
void record(A &a, V &v) {
  a("schema_version", v.schema_version);
  a("organization_id", v.organization_id);
  a("office_id", v.office_id);
  a("actor_id", v.actor_id);
  a("command_kind", v.command_kind);
  a("scope", v.scope);
  a("candidate_revision", v.candidate_revision);
  a("authority_epoch", v.authority_epoch);
  a("amount", v.amount);
  a("resource_versions", v.resource_versions);
  a("source_identity", v.source_identity);
  a("grant_id", v.grant_id);
  a("grant_revision", v.grant_revision);
  a("office_source", v.office_source);
  a("grant_source", v.grant_source);
  a("observed_at", v.observed_at);
}
// 严格遍历PopulationEnrollCommand的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,PopulationEnrollCommand>,int> =0> void record(A& a,V& v){a("authority",v.authority);a("source_id",v.source_id);a("count",v.count);a("labor_count",v.labor_count);a("release_allocation_ids",v.release_allocation_ids);a("field_order",v.field_order);}
// 严格遍历PopulationReleaseCommand的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,PopulationReleaseCommand>,int> =0> void record(A& a,V& v){a("authority",v.authority);a("enrollment_id",v.enrollment_id);a("field_order",v.field_order);}
// 严格遍历PopulationMilitaryTerms的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,PopulationMilitaryTerms>,int> =0> void record(A& a,V& v){a("source_id",v.source_id);a("organization_id",v.organization_id);a("unit_id",v.unit_id);a("garrison_id",v.garrison_id);a("place_id",v.place_id);a("household_group_id",v.household_group_id);a("reserve_cohort_id",v.reserve_cohort_id);a("serving_cohort_id",v.serving_cohort_id);a("actor_id",v.actor_id);a("military_office_id",v.military_office_id);a("military_appointment_id",v.military_appointment_id);a("authority",v.authority);a("count",v.count);a("labor_count",v.labor_count);a("member_ids",v.member_ids);a("release_allocation_ids",v.release_allocation_ids);a("enrollment_id",v.enrollment_id);}
// 严格遍历PopulationSoldierOrigin的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,PopulationSoldierOrigin>,int> =0> void record(A& a,V& v){a("member_id",v.member_id);a("source_cohort_id",v.source_cohort_id);a("household_group_id",v.household_group_id);a("labor_eligible",v.labor_eligible);}
// 严格遍历PopulationEnrollment的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,PopulationEnrollment>,int> =0> void record(A& a,V& v){a("id",v.id);a("source_id",v.source_id);a("action_id",v.action_id);a("unit_id",v.unit_id);a("actor_id",v.actor_id);a("completed_tick",v.completed_tick);a("terms",v.terms);a("members",v.members);}
// 严格遍历PopulationMilitaryAction的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,PopulationMilitaryAction>,int> =0> void record(A& a,V& v){a("action_id",v.action_id);a("kind",v.kind);a("request",v.request);a("terms",v.terms);}
// 严格遍历PopulationMilitaryActionFact的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,PopulationMilitaryActionFact>,int> =0> void record(A& a,V& v){a("id",v.id);a("kind",v.kind);a("actor_id",v.actor_id);a("phase",v.phase);a("code",v.code);a("receipt_id",v.receipt_id);a("started_tick",v.started_tick);a("completed_tick",v.completed_tick);a("success",v.success);a("request",v.request);a("terms",v.terms);}
// 严格遍历PopulationMilitaryMovement的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,PopulationMilitaryMovement>,int> =0> void record(A& a,V& v){a("member_id",v.member_id);a("enrollment_id",v.enrollment_id);a("from_cohort_id",v.from_cohort_id);a("to_cohort_id",v.to_cohort_id);a("captor_army_id",v.captor_army_id);a("before",v.before);a("after",v.after);}
// 严格遍历PopulationMilitaryFact的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,PopulationMilitaryFact>,int> =0> void record(A& a,V& v){a("id",v.id);a("kind",v.kind);a("source_id",v.source_id);a("action_id",v.action_id);a("military_receipt_id",v.military_receipt_id);a("tick",v.tick);a("movements",v.movements);a("population_receipt_ids",v.population_receipt_ids);if(a.selected_version>=3){a("occurred_at",v.occurred_at);a("military_source_schema",v.military_source_schema);}}
// 续接事实不包含第二份人口计数，只记录原军籍与工资边界。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,PopulationMilitaryIntegrationFact>,int> =0> void record(A& a,V& v){a("id",v.id);a("military_receipt_id",v.military_receipt_id);a("policy_id",v.policy_id);a("source_id",v.source_id);a("unit_id",v.unit_id);a("from_army_id",v.from_army_id);a("to_army_id",v.to_army_id);a("service_consent_source_id",v.service_consent_source_id);a("tick",v.tick);a("member_ids",v.member_ids);a("old_wage_obligation_ids",v.old_wage_obligation_ids);a("new_wage_obligation_ids",v.new_wage_obligation_ids);a("next_due_ticks",v.next_due_ticks);a("wage_remainders",v.wage_remainders);a("receiving_wage_remainders_before",v.receiving_wage_remainders_before);a("old_ration_obligation_ids",v.old_ration_obligation_ids);a("new_ration_obligation_ids",v.new_ration_obligation_ids);a("ration_provision_binding_ids",v.ration_provision_binding_ids);a("ration_next_due_ticks",v.ration_next_due_ticks);a("ration_remainders",v.ration_remainders);a("receiving_ration_remainders_before",v.receiving_ration_remainders_before);}
// 严格遍历PopulationMilitaryState的全部持久字段，缺失或多余字段均拒绝。
template<class A,class V,std::enable_if_t<std::is_same_v<std::remove_const_t<V>,PopulationMilitaryState>,int> =0> void record(A& a,V& v){a("enrollments",v.enrollments);a("actions",v.actions);a("action_facts",v.action_facts);a("facts",v.facts);a.optional("integration_facts",v.integration_facts);}
// 同一字段访问器服务冷JSON和检查点，不在热状态保留通用字典。
template<class T> BoundaryValue encode(const T&,Integer version=1);
template<class T> Status emit(ProjectionWriter&,const StableId&,const T&,Integer version=1);
template<class T> void decode(const BoundaryValue&,T&,const StableId&,Integer version=1);
template<class T> Status visit(CodecVisitor&,const StableId&,T&,Integer version=1);
struct Encoder {ProjectionWriter& writer;Integer selected_version=1;template<class T> void operator()(const char* key,const T& value){population_codec::emit(writer,key,value,selected_version);}template<class T>void optional(const char* key,const T& value){if constexpr(std::is_same_v<T,Integer>){if(value!=0)operator()(key,value);}else if(!value.empty())operator()(key,value);}};
struct Decoder {const BoundaryValue& value;StableId path;std::set<StableId> fields;Integer selected_version=1;template<class T> void operator()(const char* key,T& output){fields.insert(key);decode(boundary::field(value,key,path),output,path+"/"+key,selected_version);}template<class T>void optional(const char* key,T& output){fields.insert(key);for(const auto& row:boundary::object(value,path))if(row.first==key)decode(row.second,output,path+"/"+key,selected_version);}void finish(){std::set<StableId> actual;for(const auto& row:boundary::object(value,path)){if(!actual.insert(row.first).second)boundary::fail("DuplicatePopulationMilitaryField",path+"/"+row.first);if(!fields.count(row.first))boundary::fail("UnsupportedResidentField",path+"/"+row.first);}}};
struct Checkpointer {CodecVisitor& codec;bool integration=false;Integer selected_version=1;template<class T> void operator()(const char* key,T& value){population_codec::visit(codec,key,value,selected_version);}template<class T>void optional(const char* key,T& value){if(integration)population_codec::visit(codec,key,value,selected_version);}};
Status shape(const PopulationMilitaryState& state,Integer version,bool integration){
  if (version != 4)
    return common_failure("UnsupportedPopulationMilitaryVersion",
                          version_path(version));
  for (const auto &row : state.actions.rows())
    if (row.second.terms.authority.schema_version != 2 ||
        row.second.terms.authority.grant_id.empty())
      return common_failure("AuthorityProofVersionRequired", row.first);
  for (const auto &row : state.action_facts.rows())
    if (row.second.terms.authority.schema_version != 2 ||
        row.second.terms.authority.grant_id.empty())
      return common_failure("AuthorityProofVersionRequired", row.first);
  for (const auto &row : state.enrollments.rows())
    if (row.second.terms.authority.schema_version != 2 ||
        row.second.terms.authority.grant_id.empty())
      return common_failure("AuthorityProofVersionRequired", row.first);
  if (!integration && !state.integration_facts.empty())
    return common_failure("PopulationIntegrationCapabilityRequired",
                          "integration_facts");
  for (const auto &row : state.facts.rows()) {
    const auto &f = row.second;
    if (version < 3 && (f.occurred_at || f.military_source_schema != 1))
      return common_failure("PopulationMilitaryExactSourceVersionRequired",
                            row.first);
    if (f.military_source_schema == 1) {
      if (f.occurred_at)
        return common_failure("LegacyPopulationMilitaryHasExactMoment",
                              row.first);
    } else if (f.military_source_schema != 2 ||
               f.kind != "population_casualty" || !f.occurred_at ||
               f.occurred_at->half_day_tick != f.tick)
      return common_failure("PopulationMilitaryExactCasualtyRequired",
                            row.first);
  }
 return common_success();
}
template<class T> Status emit(ProjectionWriter& w,const StableId& key,const T& value,Integer version){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(key,value);
 else if constexpr (proof_binding_value<T>) {
   return emit_projection_record<ActorFoundationProjectionArchive>(
       w, key, value,
       [](auto &c, auto &v) { return proof_binding_fields(c, v); });
 } else if constexpr (std::is_same_v<T, WorldMoment>) {
   try {
     return emit_projection_record<ActorFoundationProjectionArchive>(
         w, key, value,
         [](auto &c, auto &v) { return exact_fields::moment(c, v); });
   } catch (const exact_fields::CodecFailure &e) {
     return w.reject(e.error.original.code, e.error.original.path);
   }
 } else if constexpr (std::is_same_v<T, std::optional<WorldMoment>>) {
   if (value)
     return population_codec::emit(w, key, *value, version);
   return w.null(key);
 } else if constexpr (std::is_same_v<T, std::uint64_t>) {
   if (value > std::uint64_t(std::numeric_limits<Integer>::max()))
     return w.reject("PopulationMilitaryVersionOverflow", key);
   return w.field(key, Integer(value));
 } else if constexpr (std::is_enum_v<T>) {
   for (const auto &row : Enum<T>::rows())
     if (row.first == value)
       return w.field(key, row.second);
   return w.reject("InvalidPopulationMilitaryEnum", key);
 } else if constexpr (Vector<T>::value)
   return w.sequence(key, value, [&](auto &child, const auto &item) {
     return population_codec::emit(child, "", item, version);
   });
 else if constexpr(Table<T>::value)return w.object(key,[&](auto& child){for(const auto& row:value.rows()){auto result=population_codec::emit(child,row.first,row.second,version);if(!result.ok)return result;}return child.status();});
 else if constexpr(std::is_same_v<T,PopulationMilitaryCommand>)return w.object(key,[&](auto& child){const bool enroll=std::holds_alternative<PopulationEnrollCommand>(value);child.field("kind",StableId(enroll?"population_enroll":"population_release"));return std::visit([&](const auto& request){return population_codec::emit(child,"params",request,version);},value);});
 else return w.object(key,[&](auto& child){Encoder a{child,version};record(a,value);return child.status();});
}
template<class T> BoundaryValue encode(const T& value,Integer version){BoundaryValue result;BoundaryProjectionWriter sink(result);auto written=population_codec::emit(sink,"",value,version);if(!written.ok)boundary::fail(written.error.original.code,written.error.original.path);written=sink.finish();if(!written.ok)boundary::fail(written.error.original.code,written.error.original.path);return result;}
template<class T> void decode(const BoundaryValue& value,T& out,const StableId& path,Integer version){
 if constexpr(std::is_same_v<T,StableId>)out=boundary::string(value,path);
 else if constexpr(std::is_same_v<T,Integer>)out=boundary::integer(value,path);
 else if constexpr(std::is_same_v<T,bool>)out=boundary::boolean(value,path);
 else if constexpr (proof_binding_value<T>) {
   ActorFoundationJsonVisitor reader(value);
   auto r = proof_binding_fields(reader, out);
   if (r.ok)
     r = reader.finish();
   if (!r.ok)
     boundary::fail(r.error.original.code, path);
 } else if constexpr (std::is_same_v<T, WorldMoment>) {
   auto r = decode_world_moment2(value);
   if (!r.ok)
     boundary::fail(r.error.original.code, r.error.original.path);
   out = std::move(r.value);
 } else if constexpr (std::is_same_v<T, std::optional<WorldMoment>>) {
   if (value.as<std::monostate>())
     out.reset();
   else {
     WorldMoment m;
     decode(value, m, path, version);
     out = std::move(m);
   }
 } else if constexpr (std::is_same_v<T, std::uint64_t>) {
   auto n = boundary::integer(value, path);
   if (n < 0)
     boundary::fail("PopulationMilitaryVersionOverflow", path);
   out = std::uint64_t(n);
 } else if constexpr (std::is_enum_v<T>) {
   auto name = boundary::string(value, path);
   for (const auto &row : Enum<T>::rows())
     if (row.second == name) {
       out = row.first;
       return;
     }
   boundary::fail("InvalidPopulationMilitaryEnum", path);
 } else if constexpr (Vector<T>::value) {
   out.clear();
   for (const auto &v : boundary::array(value, path)) {
     typename Vector<T>::Value item{};
     decode(v, item, path + "/" + std::to_string(out.size()), version);
     out.push_back(std::move(item));
   }
 } else if constexpr (Table<T>::value) {
   out = {};
   for (const auto &row : boundary::object(value, path)) {
     typename Table<T>::Value item{};
     decode(row.second, item, path + "/" + row.first, version);
     if (!out.insert(row.first, std::move(item)))
       boundary::fail("DuplicatePopulationMilitaryRecord",
                      path + "/" + row.first);
   }
 } else if constexpr (std::is_same_v<T, PopulationMilitaryCommand>) {
   boundary::fields(value, {"kind", "params"}, {}, path);
   auto kind = boundary::get_string(value, "kind", path);
   const auto &params = boundary::field(value, "params", path);
   if (kind == "population_enroll") {
     PopulationEnrollCommand request;
     decode(params, request, path + "/params", version);
     out = std::move(request);
   } else if (kind == "population_release") {
     PopulationReleaseCommand request;
     decode(params, request, path + "/params", version);
     out = std::move(request);
   } else
     boundary::fail("UnknownPopulationMilitaryCommand", path);
 } else {
   Decoder a{value, path, {}, version};
   record(a, out);
   a.finish();
 }
}
template<class T> Status visit(CodecVisitor& c,const StableId& key,T& value,Integer version){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(key,value);
 else if constexpr (proof_binding_value<T>)
   return c.object(
       key, [&](auto &child) { return proof_binding_fields(child, value); });
 else if constexpr(std::is_same_v<T,WorldMoment>)return c.object(key,[&](auto& child){return visit_world_moment2(child,value);});
 else if constexpr(std::is_same_v<T,std::optional<WorldMoment>>)return c.optional(key,value,[](auto& child,auto& item){return visit_world_moment2(child,item);});
 else if constexpr(std::is_same_v<T,std::uint64_t>){if(value>std::uint64_t(std::numeric_limits<Integer>::max()))return c.reject("PopulationMilitaryVersionOverflow",key);Integer n=Integer(value);c.field(key,n);if(n<0)return c.reject("PopulationMilitaryVersionOverflow",key);value=std::uint64_t(n);return c.status();}
 else if constexpr(std::is_enum_v<T>){StableId name;for(const auto& row:Enum<T>::rows())if(row.first==value)name=row.second;c.field(key,name);if(!c.status().ok)return c.status();for(const auto& row:Enum<T>::rows())if(row.second==name){value=row.first;return c.status();}return c.reject("InvalidPopulationMilitaryEnum",key);}
 else if constexpr(Vector<T>::value)return c.sequence(key,value,[&](auto& child,auto& v){return population_codec::visit(child,"item",v,version);});
 else if constexpr(Table<T>::value)return common_checkpoint::table(c,key,value,[&](auto& child,auto& v){return population_codec::visit(child,"record",v,version);});
 else if constexpr(std::is_same_v<T,PopulationMilitaryCommand>)return c.object(key,[&](auto& child){StableId kind=std::holds_alternative<PopulationEnrollCommand>(value)?"population_enroll":"population_release";child.field("kind",kind);if(!child.status().ok)return child.status();if(kind=="population_enroll"){PopulationEnrollCommand request;if(auto* current=std::get_if<PopulationEnrollCommand>(&value))request=*current;population_codec::visit(child,"params",request,version);value=std::move(request);}else if(kind=="population_release"){PopulationReleaseCommand request;if(auto* current=std::get_if<PopulationReleaseCommand>(&value))request=*current;population_codec::visit(child,"params",request,version);value=std::move(request);}else return child.reject("UnknownPopulationMilitaryCommand",key);return child.status();});
 else return c.object(key,[&](auto& child){Checkpointer a{child,false,version};record(a,value);return child.status();});
}
}
// 公共命令只接受有限正式参数；字段顺序元数据来自输入，不要求客户端提供。
Outcome<PopulationMilitaryCommand> decode_population_military_command(const StableId& kind,const BoundaryValue& params){try{PopulationMilitaryCommand out;if(kind=="population_enroll"){boundary::fields(params,{"authority","source_id","count","labor_count","release_allocation_ids"},{},kind);PopulationEnrollCommand value;population_codec::decode(boundary::field(params,"authority",kind),value.authority,kind+"/authority");value.source_id=boundary::get_string(params,"source_id",kind);value.count=boundary::get_int(params,"count",kind);value.labor_count=boundary::get_int(params,"labor_count",kind);value.release_allocation_ids=boundary::strings(boundary::field(params,"release_allocation_ids",kind),kind+"/release_allocation_ids");for(const auto& row:boundary::object(params,kind))value.field_order.push_back(row.first);out=std::move(value);}else if(kind=="population_release"){boundary::fields(params,{"authority","enrollment_id"},{},kind);PopulationReleaseCommand value;population_codec::decode(boundary::field(params,"authority",kind),value.authority,kind+"/authority");value.enrollment_id=boundary::get_string(params,"enrollment_id",kind);for(const auto& row:boundary::object(params,kind))value.field_order.push_back(row.first);out=std::move(value);}else return {false,{},common_failure("UnknownPopulationMilitaryCommand",kind).error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 正式报价输出实际一半日参与者与冻结来源，不额外创建页面权限。
BoundaryValue encode_population_military_quote(const PopulationMilitaryQuote& quote){return BoundaryValue{BoundaryValue::Dict{{"ok",BoundaryValue{true}},{"duration",BoundaryValue{quote.duration}},{"participants",population_codec::encode(quote.participants)},{"terms",population_codec::encode(quote.terms)}}};}
// 作者目录与旧军旅目录并列，只在显式能力分支解码。
Outcome<Integer>
selected_population_military_version(const PopulationMilitaryCatalog &catalog,
                                     bool) {
  if (catalog.selected_version != 4)
    return {
        false,
        {},
        common_failure("UnsupportedPopulationMilitaryVersion",
                       population_codec::version_path(catalog.selected_version))
            .error};
  return {true, 4, {}};
}
Outcome<PopulationMilitaryCatalog>
decode_population_military_catalog(const BoundaryValue &value) {
  try {
    PopulationMilitaryCatalog out;
    population_codec::decode(value, out, "population_military");
    for (const auto &row : boundary::object(value, "population_military"))
      if (row.first == "selected_version" && out.selected_version == 0)
        boundary::fail("UnsupportedPopulationMilitaryVersion",
                       population_codec::version_path(out.selected_version));
    if (out.selected_version != 4)
      boundary::fail("UnsupportedPopulationMilitaryVersion",
                     population_codec::version_path(out.selected_version));
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
// 冷状态缺失永久原籍不能通过补生成token修复。
Outcome<PopulationMilitaryState>
decode_population_military_state(const BoundaryValue &) {
  return {false,
          {},
          common_failure("UnsupportedPopulationMilitaryVersion",
                         population_codec::version_path(1))
              .error};
}
// 导出只读字段，保留退籍后的原籍和早先回执。
Status emit_population_military_state_fields(ProjectionWriter& writer,const PopulationMilitaryState& state){auto checked=population_codec::shape(state,1,true);if(!checked.ok)return writer.reject(checked.error.original.code,checked.error.original.path);population_codec::Encoder fields{writer};population_codec::record(fields,state);return writer.status();}
BoundaryValue encode_population_military_state(const PopulationMilitaryState& state){BoundaryValue result;BoundaryProjectionWriter sink(result);auto written=sink.object("",[&](auto& w){return emit_population_military_state_fields(w,state);});if(!written.ok)boundary::fail(written.error.original.code,written.error.original.path);written=sink.finish();if(!written.ok)boundary::fail(written.error.original.code,written.error.original.path);return result;}
// 中央选择新可选域时才调用，不向旧域追加任何尾字段。
Status visit_population_military_checkpoint(CodecVisitor &c,
                                            PopulationMilitaryState &,
                                            bool integration) {
  return c.reject("UnsupportedPopulationMilitaryVersion",
                  population_codec::version_path(integration ? 2 : 1));
}
// Implicit legacy /1 entry refuses; it cannot silently emit the new /4 body.
Status visit_population_military_checkpoint(CodecVisitor& c,PopulationMilitaryState& state){return visit_population_military_checkpoint(c,state,false);}

Outcome<PopulationMilitaryState>
decode_population_military_state(const BoundaryValue &value, Integer version,
                                 bool integration) {
  try {
    if (version != 4)
      boundary::fail("UnsupportedPopulationMilitaryVersion",
                     population_codec::version_path(version));
    PopulationMilitaryState out;
    population_codec::decode(value, out, "population_military", version);
    auto checked = population_codec::shape(out, version, integration);
    if (!checked.ok)
      boundary::fail(checked.error.original.code, checked.error.original.path);
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status
emit_population_military_state_fields(ProjectionWriter &writer,
                                      const PopulationMilitaryState &state,
                                      Integer version, bool integration) {
  if (version != 4)
    return writer.reject("UnsupportedPopulationMilitaryVersion",
                         population_codec::version_path(version));
  auto checked = population_codec::shape(state, version, integration);
  if (!checked.ok)
    return writer.reject(checked.error.original.code,
                         checked.error.original.path);
  population_codec::Encoder fields{writer, version};
  population_codec::record(fields, state);
  return writer.status();
}
BoundaryValue encode_population_military_state(const PopulationMilitaryState& state,Integer version,bool integration){BoundaryValue result;BoundaryProjectionWriter sink(result);auto written=sink.object("",[&](auto& w){return emit_population_military_state_fields(w,state,version,integration);});if(!written.ok)boundary::fail(written.error.original.code,written.error.original.path);written=sink.finish();if(!written.ok)boundary::fail(written.error.original.code,written.error.original.path);return result;}
Status visit_population_military_checkpoint(CodecVisitor &c,
                                            PopulationMilitaryState &state,
                                            Integer version, bool integration) {
  if (version != 4)
    return c.reject("UnsupportedPopulationMilitaryVersion",
                    population_codec::version_path(version));
  if (!c.reading()) {
    auto checked = population_codec::shape(state, version, integration);
    if (!checked.ok)
      return c.reject(checked.error.original.code, checked.error.original.path);
  }
  population_codec::Checkpointer fields{c, integration, version};
  population_codec::record(fields, state);
  if (c.status().ok) {
    auto checked = population_codec::shape(state, version, integration);
    if (!checked.ok)
      return c.reject(checked.error.original.code, checked.error.original.path);
  }
  return c.status();
}
} // namespace hanmo::resident

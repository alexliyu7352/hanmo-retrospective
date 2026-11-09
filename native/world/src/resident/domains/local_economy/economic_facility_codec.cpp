// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 原09/P08字段只列一处。JSON与bounded shape同emit；二进制有限union先tag再固定payload。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "production/facility_condition_fields.hpp"
#include "economic_facility_provider.hpp"
#include "economic_facility_fields.hpp"
#include "economic_facility_reference_fields.hpp"
#include "resident/kernel/native_catalog_decode.hpp"
#include "resident/checkpoint/canonical_digest.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/storage/common_projection.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include <limits>
#include <type_traits>
#include <tuple>
#include <string_view>
namespace hanmo::resident::facility_codec {
template<class T>struct IsVariant:std::false_type{};template<class...T>struct IsVariant<std::variant<T...>>:std::true_type{};
template<class T>struct IsVector:std::false_type{};template<class T>struct IsVector<std::vector<T>>:std::true_type{using Value=T;};
template<class T>struct IsOptional:std::false_type{};template<class T>struct IsOptional<std::optional<T>>:std::true_type{using Value=T;};
template<class T>struct IsTable:std::false_type{};template<class T>struct IsTable<OrderedTable<T>>:std::true_type{using Value=T;};

// The five retained owner tables have one owning vector each. The wire view is
// the fixed three-owner partition; export does not copy or reorder the world.
template<class T>struct OwnerRows {std::vector<T>& rows;};
template<class T>struct IsOwnerRows:std::false_type{};
template<class T>struct IsOwnerRows<OwnerRows<T>>:std::true_type{using Value=T;};
constexpr const char* owner_names[]={"local_economy","estate","commerce"};
std::string_view owner_of(const EconomicFacilitySlotDefinition&v){switch(v.asset_kind){case EconomicFacilityAssetKind::production_asset_facility:return owner_names[0];case EconomicFacilityAssetKind::estate_service_facility:return owner_names[1];case EconomicFacilityAssetKind::business_service_facility:return owner_names[2];}return {};}
std::string_view owner_of(const PhysicalServiceFacilityExtension&v){return v.asset_owner_domain;}
std::string_view owner_of(const FacilityPhysicalClaimBinding&v){return v.slot_ref.provider_domain;}
std::string_view owner_of(const EconomicFacilityCapacitySource&v){return v.slot_ref.provider_domain;}
std::string_view owner_of(const FacilityRetentionTransition&v){return v.slot_ref.provider_domain;}
bool wire_less(const EconomicFacilitySlotDefinition&a,const EconomicFacilitySlotDefinition&b){return std::tie(a.definition_ref.id,a.definition_ref.version,a.definition_ref.catalog_digest)<std::tie(b.definition_ref.id,b.definition_ref.version,b.definition_ref.catalog_digest);}
bool wire_less(const PhysicalServiceFacilityExtension&a,const PhysicalServiceFacilityExtension&b){return a.facility_id<b.facility_id;}
bool wire_less(const FacilityPhysicalClaimBinding&a,const FacilityPhysicalClaimBinding&b){return a.claim_id<b.claim_id;}
bool wire_less(const EconomicFacilityCapacitySource&a,const EconomicFacilityCapacitySource&b){return std::tie(a.slot_ref.provider_domain,a.slot_ref.facility_id,a.slot_ref.slot_kind,a.source_revision)<std::tie(b.slot_ref.provider_domain,b.slot_ref.facility_id,b.slot_ref.slot_kind,b.source_revision);}
bool wire_less(const FacilityRetentionTransition&a,const FacilityRetentionTransition&b){return a.transition_id<b.transition_id;}
template<class T>Status validate_owner_rows(const std::vector<T>&rows){for(std::size_t i=0;i<rows.size();++i){const auto owner=owner_of(rows[i]);if(owner!=owner_names[0]&&owner!=owner_names[1]&&owner!=owner_names[2])return common_failure("UnknownEconomicFacilityOwner");for(std::size_t j=0;j<i;++j)if(!wire_less(rows[i],rows[j])&&!wire_less(rows[j],rows[i]))return common_failure("DuplicateEconomicFacilityWireKey");}return common_success();}
Status validate_wire_state(const EconomicFacilitySourcesState&v){if(v.schema_version!=1)return common_failure("UnsupportedEconomicFacilitySourceSchema");auto status=validate_owner_rows(v.definitions);if(status.ok)status=validate_owner_rows(v.facilities);if(status.ok)status=validate_owner_rows(v.physical_bindings);if(status.ok)status=validate_owner_rows(v.capacity_sources);if(status.ok)status=validate_owner_rows(v.retention_transitions);return status;}
// A borrowed canonical range. Each increment scans the immutable source; no
// temporary pointer/index table escapes the bounded projection/checkpoint job.
template<class T>struct CanonicalOwnerRange {
 const std::vector<T>&rows;std::string_view owner;
 const T* next(const T*previous)const{const T*best=nullptr;for(const auto&row:rows)if(owner_of(row)==owner&&(!previous||wire_less(*previous,row))&&(!best||wire_less(row,*best)))best=&row;return best;}
 std::size_t size()const{std::size_t n=0;for(const auto&row:rows)if(owner_of(row)==owner)++n;return n;}
 struct Iterator{const CanonicalOwnerRange*range;const T*row;const T&operator*()const{return *row;}Iterator&operator++(){row=range->next(row);return *this;}bool operator!=(const Iterator&other)const{return row!=other.row;}};
 Iterator begin()const{return {this,next(nullptr)};}Iterator end()const{return {this,nullptr};}
};
template<class T>struct EnumLimit;
template<>struct EnumLimit<EconomicFacilityAssetKind>{static constexpr Integer count=3;};
template<>struct EnumLimit<EconomicFacilitySlotKind>{static constexpr Integer count=4;};
template<>struct EnumLimit<ProductionAssetKind>{static constexpr Integer count=2;};
template<>struct EnumLimit<ProductionFacilityKind>{static constexpr Integer count=3;};
template<std::size_t I=0,class C,class T>Status select_variant(C& archive,T& value,std::size_t index){
 if constexpr(I<std::variant_size_v<T>){if(index==I){auto valid=archive.template prepare_default_values<std::variant_alternative_t<I,T>>();if(!valid.ok)return valid;value.template emplace<I>();return archive.status();}return select_variant<I+1>(archive,value,index);}
 else return archive.reject("UnsupportedFacilityVariant","");
}
template<class A>void record(A& a,std::pair<StableId,StableId>& v){a("name",v.first);a("version",v.second);}
template<class A>void record(A& a,SourceRef& v){a("domain",v.domain);a("kind",v.kind);a("id",v.id);}
template<class A>void record(A& a,WorldMoment& v){a("half_day_tick",v.half_day_tick);a("sub_half_day",v.sub_half_day);}
template<class A>void record(A& a,MomentSpan& v){a("from",v.from);a("to",v.to);}
template<class A>void record(A& a,ActorFoundationRuleRef& v){a("id",v.id);a("version",v.version);a("catalog_digest",v.catalog_digest);}
template<class A>void record(A& a,CatalogIdentity& v){a("content_id",v.content_id);a("content_version",v.content_version);a("content_digest",v.content_digest);a("ruleset_id",v.ruleset_id);a("schema_sha256",v.schema_sha256);a("pack_set_digest",v.pack_set_digest);a("module_versions",v.module_versions);a("artifact_sha256",v.artifact_sha256);}
template<class A>void record(A& a,FacilitySlotRef& v){a("provider_domain",v.provider_domain);a("facility_id",v.facility_id);a("slot_kind",v.slot_kind);}
template<class A>void record(A& a,OriginalConditionBaseSlot& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,InstalledServiceUnits& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,ActivatedServiceUnits& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityPhysicalUnitDefinition& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,AggregateFacilityLayout& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,IndependentFacilityUnits& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,SharedFacilitySafetyZone& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,EconomicFacilitySlotDefinition& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilitySeedRowRef& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityInitializationAdoptionIdentity& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,PhysicalFacilityInstalledEvidence& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,InstalledPhysicalFacility& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,ActivatedPhysicalFacility& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,PhysicalServiceFacilityExtension& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityPhysicalClaimBinding& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityRetentionBegin& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityRetentionContinue& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityRetentionClear& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityRetentionRestoredService& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityRetentionTransition& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityNotConditioned& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityProductionCondition& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,OriginalFacilityOpenRule& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,AcceptedFacilityOpeningIntervals& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,EconomicFacilityUsageRightBasis& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityInitializedCause& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityActivatedCause& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityConditionChangedCause& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityOpeningChangedCause& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityRightChangedCause& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,FacilityRetentionChangedCause& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,AdmitWithinCapacity& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,NormalWithRetentions& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,EntireSlotNoNewAdmission& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,EconomicFacilityCapacitySource& v){economic_facility_fields::fields(a,v);}
template<class A>struct OwnerPartitionFields{
 A& archive;
 template<class T>void operator()(const char* name,T& value){if constexpr(IsVector<T>::value){OwnerRows<typename IsVector<T>::Value> rows{value};archive(name,rows);}else archive(name,value);}
};
template<class A>void record(A& a,EconomicFacilitySourcesState& v){OwnerPartitionFields<A> fields{a};economic_facility_fields::fields(fields,v);}
template<class A>void record(A& a,ProductionAsset& v){a("id",v.id);a("site_id",v.site_id);a("source_key",v.source_key);a("owner",v.owner);a("kind",v.kind);a("capacity_units",v.capacity_units);}
template<class A>void record(A& a,EstateFacilities& v){a("farm",v.farm);a("granary",v.granary);a("palisade",v.palisade);a("field_order",v.field_order);}
template<class A>void record(A& a,EstateConstruction& v){a("build_ticks",v.build_ticks);a("material_cost",v.material_cost);a("max_level",v.max_level);a("money_cost",v.money_cost);a("material_item_id",v.material_item_id);a("payee_wallet_id",v.payee_wallet_id);}
template<class A>void record(A& a,EstateDefinition& v){a("id",v.id);a("food_item_id",v.food_item_id);a("inventory_id",v.inventory_id);a("manager_actor_id",v.manager_actor_id);a("output_item_id",v.output_item_id);a("owner_actor_id",v.owner_actor_id);a("place_id",v.place_id);a("wallet_id",v.wallet_id);a("granary_capacity_per_level",v.granary_capacity_per_level);a("output_per_worker",v.output_per_worker);a("palisade_defense_per_guard",v.palisade_defense_per_guard);a("ration_per_person",v.ration_per_person);a("base_capacity",v.base_capacity);a("resident_actor_ids",v.resident_actor_ids);a("initial_facilities",v.initial_facilities);a("initial_duties",v.initial_duties);a("construction",v.construction);}
template<class A>void record(A& a,PrivateShopPremises& v){a("id",v.id);a("place_id",v.place_id);a("lessor_actor_id",v.lessor_actor_id);a("lessor_wallet_id",v.lessor_wallet_id);a("lessor_role_id",v.lessor_role_id);a("agreement_dialogue_id",v.agreement_dialogue_id);a("agreement_node_id",v.agreement_node_id);a("market_shop_id",v.market_shop_id);a("opening_fee",v.opening_fee);a("initial_capital",v.initial_capital);a("inventory_capacity",v.inventory_capacity);a("daily_rent",v.daily_rent);a("rent_rate_profile",v.rent_rate_profile);}
template<class A>void record(A& a,EconomicFacilityInitialSeed& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,EconomicFacilityOpenRule& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,EconomicFacilityUsagePolicy& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,EconomicFacilityCatalog& v){economic_facility_fields::fields(a,v);}
template<class A>struct AuthoredCatalogFields {
 A& archive;
 template<class T>void operator()(const char* name,T& value){if constexpr(!std::is_same_v<T,CatalogIdentity>)archive(name,value);}
};
template<class A>void record(A& a,FacilityBaseSlotCapacity& v){facility_condition_fields::fields(a,v);}
template<class A>void record(A& a,ProductionFacilityCondition& v){facility_condition_fields::fields(a,v);}
template<class A>void record(A& a,ProductionFacilityConditionSeed& v){facility_condition_fields::fields(a,v);}
template<class A>void record(A& a,FacilityAttackResultRef& v){facility_condition_fields::fields(a,v);}
template<class A>void record(A& a,CompletedFacilityRepairRef& v){facility_condition_fields::fields(a,v);}
template<class A>void record(A& a,FacilitySlotCapacityChange& v){facility_condition_fields::fields(a,v);}
template<class A>void record(A& a,FacilityConditionChange& v){facility_condition_fields::fields(a,v);}
template<class A>void record(A& a,ProductionFacilityConditionsState& v){economic_facility_fields::fields(a,v);}
template<class A>void record(A& a,ProductionFacilityConditionsCatalog& v){economic_facility_fields::fields(a,v);}
template<class T>Status project(ProjectionWriter&,std::string_view,const T&);template<class T>void decode(const BoundaryValue&,T&,std::string_view);template<class T>Status checkpoint(CodecVisitor&,std::string_view,T&);
struct Projector{ProjectionWriter& w;template<class T>void operator()(const char* k,const T& v){project(w,k,v);}};
template<class T>inline constexpr bool json_scalar=std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>||std::is_same_v<T,std::uint32_t>||std::is_enum_v<T>;
// 仅作者入口携带真实目录身份；冷读和运行封套没有此上下文。
struct AuthoredDecodeContext {
 const CatalogIdentity& identity;
 const EconomicFacilityCatalog* facilities=nullptr;
};
template<class T>Status json_value(ActorFoundationJsonVisitor&,const BoundaryValue&,T&,std::string_view,const AuthoredDecodeContext* =nullptr);
template<class T>Status json_field(ActorFoundationJsonVisitor&,const BoundaryValue&,std::string_view,T&,std::string_view,const AuthoredDecodeContext* =nullptr);
inline Status checked(Status value){if(!value.ok)throw boundary::DecodeFailure{value.error};return value;}
inline StableId index_path(std::string_view path,std::size_t index){char digits[3*sizeof(index)+1];auto* end=digits+sizeof(digits);auto* start=end;do{*--start=char('0'+index%10);index/=10;}while(index);return checkpoint_decode_boundary_join({path,"/",std::string_view(start,std::size_t(end-start))});}
struct Decoder{
 ActorFoundationJsonVisitor& archive;const BoundaryValue& input;std::string_view path;const AuthoredDecodeContext* authored=nullptr;
 template<class T>void operator()(const char* name,T& value){checked(json_field(archive,input,name,value,path,authored));}
 void finish(){for(const auto& row:boundary::object(input,path))if(!archive.has_visited_field(row.first))boundary::fail_parts("UnsupportedResidentField",{path,"/",row.first});checked(archive.finish());}
};
// These are the actual original owner-partition and condition-table rows.
// Ordinary fields move; any exact fraction copies through its genuine owner.
// A failed transfer poisons the same private candidate, never a saved root.
template<class T>inline constexpr bool exact_transfer_row=std::is_same_v<T,EconomicFacilitySlotDefinition>||std::is_same_v<T,PhysicalServiceFacilityExtension>||std::is_same_v<T,FacilityPhysicalClaimBinding>||std::is_same_v<T,EconomicFacilityCapacitySource>||std::is_same_v<T,FacilityRetentionTransition>||std::is_same_v<T,FacilityConditionChange>||std::is_same_v<T,ProductionFacilityCondition>;
template<class T>struct TransferRow{
 template<class Destination,class Row>Status operator()(Destination& destination,Row& source)const{static_assert(exact_transfer_row<T>,"Original finite facility row transfer required");destination.emplace_back(std::move(source));return common_success();}
};
struct Checkpointer{CodecVisitor& c;template<class T>void operator()(const char* k,T& v){checkpoint(c,k,v);}};
[[noreturn]]void raise(const RuleError& e){boundary::DecodeFailure failure{e.original.code,e.original.path};failure.error=e;throw failure;}
template<class T>Status project(ProjectionWriter& w,std::string_view name,const T& v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(name,v);
 else if constexpr(std::is_same_v<T,std::uint32_t>)return w.field(name,static_cast<Integer>(v));
 else if constexpr(std::is_same_v<T,Owner>)return emit_common_owner(w,name,v);
 else if constexpr(std::is_same_v<T,WorldId>)return w.sequence(name,v,[](auto& x,const auto& b){return x.field("",static_cast<Integer>(b));});
 else if constexpr(std::is_same_v<T,CanonicalFraction>){auto e=encode_canonical_fraction(v);if(!e.ok)return w.reject("InvalidFacilityFraction",name);return w.object(name,[&](auto& x){x.field("numerator",e.value.numerator);return x.field("denominator",e.value.denominator);});}
 else if constexpr(std::is_enum_v<T>){auto tag=static_cast<Integer>(v);if(tag<0||tag>=EnumLimit<T>::count)return w.reject("UnknownFacilityEnum",name);return w.field(name,tag);}
 else if constexpr(IsVariant<T>::value)return w.object(name,[&](auto& x){x.field("tag",static_cast<Integer>(v.index()));return std::visit([&](const auto& payload){Projector a{x};record(a,const_cast<std::decay_t<decltype(payload)>&>(payload));return x.status();},v);});
 else if constexpr(IsOwnerRows<T>::value){auto valid=validate_owner_rows(v.rows);if(!valid.ok)return w.reject(valid.error.original.code,name);return w.object(name,[&](auto&x){for(const auto*owner:owner_names){CanonicalOwnerRange<typename IsOwnerRows<T>::Value> range{v.rows,owner};x.sequence(owner,range,[](auto&out,const auto&item){return project(out,"",item);});if(!x.status().ok)break;}return x.status();});}
 else if constexpr(IsVector<T>::value)return w.sequence(name,v,[](auto& x,const auto& item){return project(x,"",item);});
 else if constexpr(IsOptional<T>::value){if(v)return project(w,name,*v);return w.null(name);}
 else if constexpr(IsTable<T>::value)return w.keyed_dictionary(name,{},[&](auto& x){for(const auto& p:v.rows()){project(x,p.first,p.second);if(!x.status().ok)break;}return x.status();});
 else return w.object(name,[&](auto& x){Projector a{x};record(a,const_cast<T&>(v));return x.status();});
}
template<class T>Status json_value(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,T& value,std::string_view path,const AuthoredDecodeContext* authored){
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(input,{path});return archive.field("",value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,std::uint32_t>){auto number=boundary::integer(input,path);if(number<0||static_cast<std::uint64_t>(number)>std::numeric_limits<std::uint32_t>::max())boundary::fail("InvalidFacilitySchema",path);checked(archive.field("",number));value=static_cast<std::uint32_t>(number);return archive.status();}
 else if constexpr(std::is_enum_v<T>){auto tag=boundary::integer(input,path);if(tag<0||tag>=EnumLimit<T>::count)boundary::fail("UnknownFacilityEnum",path);checked(archive.field("",tag));value=static_cast<T>(tag);return archive.status();}
 else if constexpr(std::is_same_v<T,ActorFoundationRuleRef>){
  checked(archive.defer_duplicate_field_check_to_finish());Decoder fields{archive,input,path,authored};
  if(!authored){record(fields,value);fields.finish();return archive.status();}
  fields("id",value.id);fields("version",value.version);
  if(input.find("catalog_digest"))fields("catalog_digest",value.catalog_digest);
  else value.catalog_digest=checkpoint_decode_boundary_string(authored->identity.content_digest);
  fields.finish();
  if(value.id.empty()||value.version<=0||authored->identity.content_digest.empty()||value.catalog_digest!=authored->identity.content_digest)
   boundary::fail("AuthoredFacilityRuleIdentityMismatch",path);
  return archive.status();
 }
 else if constexpr(std::is_same_v<T,FacilitySeedRowRef>){
  checked(archive.defer_duplicate_field_check_to_finish());Decoder fields{archive,input,path,authored};
  if(!authored){record(fields,value);fields.finish();return archive.status();}
  fields("owner_domain",value.owner_domain);fields("seed_kind",value.seed_kind);fields("seed_row_key",value.seed_row_key);
  const bool explicit_digest=input.find("seed_row_digest")!=nullptr;
  if(explicit_digest)fields("seed_row_digest",value.seed_row_digest);
  fields.finish();
  if(explicit_digest&&value.seed_row_digest.empty())boundary::fail("AuthoredFacilitySeedDigestMismatch",path);
  if(authored->facilities){
   const FacilitySeedRowRef* original=nullptr;
   for(const auto& seed:authored->facilities->initial_rows)
    if(seed.row_ref.owner_domain==value.owner_domain&&seed.row_ref.seed_kind==value.seed_kind&&seed.row_ref.seed_row_key==value.seed_row_key){
     if(original)boundary::fail("DuplicateFacilitySeedRow",path);
     original=&seed.row_ref;
    }
   if(!original||original->seed_row_digest.empty()||(explicit_digest&&value.seed_row_digest!=original->seed_row_digest))
    boundary::fail("AuthoredFacilitySeedDigestMismatch",path);
   if(!explicit_digest)value.seed_row_digest=checkpoint_decode_boundary_string(original->seed_row_digest);
  }
  return archive.status();
 }
 else if constexpr(std::is_same_v<T,Owner>||std::is_same_v<T,WorldId>){
  auto original=[&](const BoundaryValue& node)->Outcome<T>{try{
   if constexpr(std::is_same_v<T,Owner>)return {true,boundary::decode_owner(node,checkpoint_decode_boundary_text(path)),{}};
   else{T out{};const auto& bytes=boundary::array(node,path);if(bytes.size()!=out.size())boundary::fail("InvalidWorldId",path);for(std::size_t i=0;i<bytes.size();++i){auto byte=boundary::integer(bytes[i],path);if(byte<0||byte>255)boundary::fail("InvalidWorldId",path);out[i]=static_cast<std::uint8_t>(byte);}return {true,std::move(out),{}};}
  }catch(const boundary::DecodeFailure& error){return {false,{},error.error};}};
  auto emit_original=[](ProjectionWriter& writer,const T& row){return project(writer,"",row);};
  if(ScopedCheckpointJsonDecode::current())return archive.foreign_record(value,[&](const BoundaryValue& node,ScopedCheckpointJsonDecode& budget)->Outcome<T>{if(&budget!=ScopedCheckpointJsonDecode::current())budget.fail("CheckpointCandidateMemoryJobMismatch","facility_original");budget.template default_values<T>();return original(node);},emit_original);
  return archive.foreign_record(value,original,emit_original);
 }
 else if constexpr(std::is_same_v<T,CanonicalFraction>){
  boundary::fields(input,{"numerator","denominator"},{},path);checked(archive.defer_duplicate_field_check_to_finish());EncodedFraction encoded;
  checked(json_field(archive,input,"numerator",encoded.numerator,path,authored));checked(json_field(archive,input,"denominator",encoded.denominator,path,authored));
  auto decoded=decode_canonical_fraction(encoded);if(!decoded.ok)raise(decoded.error);value=std::move(decoded.value);return archive.finish();
 }
 else if constexpr(IsVariant<T>::value){
  auto tag=boundary::get_int(input,"tag",path);if(tag<0||static_cast<std::size_t>(tag)>=std::variant_size_v<T>)boundary::fail("UnsupportedFacilityVariant",path);
  checked(archive.defer_duplicate_field_check_to_finish());checked(archive.field("tag",tag));checked(select_variant(archive,value,static_cast<std::size_t>(tag)));
  std::visit([&](auto& payload){Decoder fields{archive,input,path,authored};record(fields,payload);fields.finish();},value);return archive.status();
 }
 else if constexpr(IsOwnerRows<T>::value){
  boundary::fields(input,{"local_economy","estate","commerce"},{},path);checked(archive.defer_duplicate_field_check_to_finish());value.rows.clear();
  using Row=typename IsOwnerRows<T>::Value;
  for(const auto* owner:owner_names){const auto owner_path=checkpoint_decode_boundary_join({path,"/",owner});const auto& rows=boundary::array(boundary::field(input,owner,path),owner_path);const auto start=value.rows.size();std::size_t index=0;
   checked(archive.append_sequence(owner,value.rows,[&](auto& child,auto& row){const auto position=index++;checked(json_value(child,rows[position],row,index_path(owner_path,position),authored));if(owner_of(row)!=owner)boundary::fail("EconomicFacilityOwnerTableMismatch",owner_path);if(position&&!wire_less(value.rows[start+position-1],row))boundary::fail("NonCanonicalEconomicFacilityRows",owner_path);return child.status();},TransferRow<Row>{}));
  }
  auto valid=validate_owner_rows(value.rows);if(!valid.ok)raise(valid.error);return archive.finish();
 }
 else if constexpr(IsVector<T>::value){const auto& rows=boundary::array(input,path);std::size_t index=0;return archive.root_sequence(value,path,[&](auto& child,auto& row){const auto position=index++;return json_value(child,rows[position],row,index_path(path,position),authored);},json_scalar<typename IsVector<T>::Value>);}
 else if constexpr(IsOptional<T>::value)return archive.root_optional(value,path,[&](auto& child,auto& row){return json_value(child,input,row,path,authored);},json_scalar<typename IsOptional<T>::Value>);
 else{checked(archive.defer_duplicate_field_check_to_finish());Decoder fields{archive,input,path,authored};record(fields,value);fields.finish();
  if constexpr(std::is_same_v<T,EconomicFacilityInitialSeed>){if(authored){
   const auto& original=boundary::field(input,"original_asset_row",path);
   // 唯一共享叶负责字节算法；此处先保真实输入复制及64字节摘要，JSON临时树仍待共享叶接账。
   auto temporary=CheckpointMemoryCharge(0);
   const auto digest=boundary_canonical_digest(original);
   if(value.row_ref.seed_row_digest.empty())value.row_ref.seed_row_digest=checkpoint_decode_boundary_string(digest);
   else if(value.row_ref.seed_row_digest!=digest)boundary::fail("AuthoredFacilitySeedDigestMismatch",path);
  }}
  return archive.status();}
}
template<class T>Status json_field(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,std::string_view name,T& value,std::string_view path,const AuthoredDecodeContext* authored){
 const auto& node=boundary::field(input,name,path);const auto child_path=checkpoint_decode_boundary_join({path,"/",name});
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(node,{child_path});return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,std::uint32_t>){auto number=boundary::integer(node,child_path);if(number<0||static_cast<std::uint64_t>(number)>std::numeric_limits<std::uint32_t>::max())boundary::fail("InvalidFacilitySchema",child_path);checked(archive.field(name,number));value=static_cast<std::uint32_t>(number);return archive.status();}
 else if constexpr(std::is_enum_v<T>){auto tag=boundary::integer(node,child_path);if(tag<0||tag>=EnumLimit<T>::count)boundary::fail("UnknownFacilityEnum",child_path);checked(archive.field(name,tag));value=static_cast<T>(tag);return archive.status();}
 else if constexpr(IsVector<T>::value){const auto& rows=boundary::array(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;return archive.sequence(name,value,[&](auto& child,auto& row){const auto position=index++;return json_value(child,rows[position],row,index_path(child_path,position),authored);},json_scalar<typename IsVector<T>::Value>);}
 else if constexpr(IsOptional<T>::value)return archive.optional(name,value,[&](auto& child,auto& row){return json_value(child,node,row,child_path,authored);},json_scalar<typename IsOptional<T>::Value>);
 else if constexpr(IsTable<T>::value){const auto& rows=boundary::object(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;auto read=[&](auto& child,auto& row){const auto position=index++;auto result=json_value(child,rows[position].second,row,checkpoint_decode_boundary_join({child_path,"/",rows[position].first}),authored);for(std::size_t prior=0;prior<position;++prior)if(rows[prior].first==rows[position].first)boundary::fail_parts("DuplicateFacilityRecord",{child_path,"/",rows[position].first});return result;};using Row=typename IsTable<T>::Value;if constexpr(exact_transfer_row<Row>)return archive.table(name,value,read,TransferRow<Row>{},json_scalar<Row>);else return archive.table(name,value,read,json_scalar<Row>);}
 else return archive.object(name,[&](auto& child){return json_value(child,node,value,child_path,authored);});
}
template<class T>void decode(const BoundaryValue& input,T& value,std::string_view path){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(json_value(archive,input,value,path));checked(archive.finish());}
template<class T>T decode_root(const BoundaryValue& input,std::string_view path){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(archive.template prepare_default_values<T>());T value;checked(json_value(archive,input,value,path));checked(archive.finish());return value;}
template<class T>Status checkpoint(CodecVisitor& c,std::string_view name,T& v){
 if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,v);
 else if constexpr(std::is_same_v<T,std::uint32_t>){Integer n=v;c.field(name,n);if(n<0||static_cast<std::uint64_t>(n)>std::numeric_limits<std::uint32_t>::max())return c.reject("InvalidFacilitySchema",name);if(c.reading())v=static_cast<std::uint32_t>(n);return c.status();}
 else if constexpr(std::is_same_v<T,Owner>)return c.object(name,[&](auto& x){return common_checkpoint::owner(x,v);});
 else if constexpr(std::is_same_v<T,WorldId>){if(c.reading()){std::vector<Integer> bytes;c.sequence(name,bytes,[](auto& x,auto& b){return x.field("byte",b);});if(!c.status().ok)return c.status();if(bytes.size()!=v.size())return c.reject("InvalidWorldId",name);for(std::size_t i=0;i<bytes.size();++i){if(bytes[i]<0||bytes[i]>255)return c.reject("InvalidWorldId",name);v[i]=static_cast<std::uint8_t>(bytes[i]);}return c.status();}return c.sequence_view(name,v.size(),[&](auto& x,std::uint64_t i){Integer byte=v[static_cast<std::size_t>(i)];return x.field("byte",byte);});}
 else if constexpr(std::is_same_v<T,CanonicalFraction>)return c.object(name,[&](auto& x){EncodedFraction e;if(!x.reading()){auto value=encode_canonical_fraction(v);if(!value.ok)return x.reject("InvalidFacilityFraction",name);e=value.value;}x.field("numerator",e.numerator);x.field("denominator",e.denominator);if(x.reading()&&x.status().ok){auto value=decode_canonical_fraction(e);if(!value.ok)return x.reject("InvalidFacilityFraction",name);v=value.value;}return x.status();});
 else if constexpr(std::is_enum_v<T>){Integer tag=static_cast<Integer>(v);c.field(name,tag);if(!c.status().ok)return c.status();if(tag<0||tag>=EnumLimit<T>::count)return c.reject("UnknownFacilityEnum",name);if(c.reading())v=static_cast<T>(tag);return c.status();}
 else if constexpr(IsVariant<T>::value)return c.object(name,[&](auto& x){Integer tag=static_cast<Integer>(v.index());x.field("tag",tag);if(!x.status().ok)return x.status();if(tag<0||static_cast<std::size_t>(tag)>=std::variant_size_v<T>)return x.reject("UnsupportedFacilityVariant",name);if(x.reading()){auto selected=select_variant(x,v,static_cast<std::size_t>(tag));if(!selected.ok)return selected;}return std::visit([&](auto& payload){Checkpointer a{x};record(a,payload);return x.status();},v);});
 else if constexpr(IsOwnerRows<T>::value){
  if(!c.reading()){auto valid=validate_owner_rows(v.rows);if(!valid.ok)return c.reject(valid.error.original.code,name);}
  return c.object(name,[&](auto&x){if(x.reading())v.rows.clear();for(const auto*owner:owner_names){
   if(x.reading()){const auto start=v.rows.size();std::size_t current=start;x.append_sequence(owner,v.rows,[&](auto&out,auto&item){checkpoint(out,"item",item);if(!out.status().ok)return out.status();if(owner_of(item)!=owner)return out.reject("EconomicFacilityOwnerTableMismatch",owner);if(current>start&&!wire_less(v.rows[current-1],item))return out.reject("NonCanonicalEconomicFacilityRows",owner);++current;return out.status();},TransferRow<typename IsOwnerRows<T>::Value>{});}
   else{CanonicalOwnerRange<typename IsOwnerRows<T>::Value>range{v.rows,owner};x.write_sequence(owner,range,[&](auto&out,const auto&item){return checkpoint(out,"item",const_cast<typename IsOwnerRows<T>::Value&>(item));});}
   if(!x.status().ok)return x.status();}
   auto valid=validate_owner_rows(v.rows);if(!valid.ok)return x.reject(valid.error.original.code,name);return x.status();});}
 else if constexpr(IsVector<T>::value)return c.sequence(name,v,[&](auto& x,auto& item){return checkpoint(x,"item",item);});
 else if constexpr(IsOptional<T>::value)return c.optional(name,v,[&](auto& x,auto& item){return checkpoint(x,"value",item);});
 else if constexpr(IsTable<T>::value)return common_checkpoint::table(c,name,v,[&](auto& x,auto& item){return checkpoint(x,"record",item);});
 else return c.object(name,[&](auto& x){Checkpointer a{x};record(a,v);return x.status();});
}
template<class T>BoundaryValue encode(const T& v){BoundaryValue output;BoundaryProjectionWriter w(output);auto status=project(w,"",v);if(status.ok)status=w.finish();if(!status.ok)raise(status.error);return output;}
}
namespace hanmo::resident {
BoundaryValue encode_economic_facility_sources(const EconomicFacilitySourcesState& v){auto valid=facility_codec::validate_wire_state(v);if(!valid.ok)facility_codec::raise(valid.error);return facility_codec::encode(v);}
Outcome<EconomicFacilitySourcesState> decode_economic_facility_sources(const BoundaryValue& v){try{auto out=facility_codec::decode_root<EconomicFacilitySourcesState>(v,"/economic_facility_sources");auto valid=facility_codec::validate_wire_state(out);if(!valid.ok)return {false,{},valid.error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status emit_economic_facility_sources(ProjectionWriter& w,const EconomicFacilitySourcesState& v){auto valid=facility_codec::validate_wire_state(v);if(!valid.ok)return w.reject(valid.error.original.code,"economic_facility_sources");return facility_codec::project(w,"",v);}
Status visit_economic_facility_sources(CodecVisitor& c,EconomicFacilitySourcesState& v){if(!c.reading()){auto valid=facility_codec::validate_wire_state(v);if(!valid.ok)return c.reject(valid.error.original.code,"economic_facility_sources");}facility_codec::Checkpointer a{c};facility_codec::record(a,v);if(c.status().ok){auto valid=facility_codec::validate_wire_state(v);if(!valid.ok)return c.reject(valid.error.original.code,"economic_facility_sources");}return c.status();}
BoundaryValue encode_economic_facility_catalog(const EconomicFacilityCatalog& v){return facility_codec::encode(v);}
Outcome<EconomicFacilityCatalog> decode_economic_facility_catalog(const BoundaryValue& v){try{auto out=facility_codec::decode_root<EconomicFacilityCatalog>(v,"/economic_facility_catalog");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<EconomicFacilityCatalog> decode_authored_economic_facility_catalog(const BoundaryValue& input,const CatalogIdentity& identity){try{
 ActorFoundationJsonVisitor archive(input);facility_codec::checked(archive.template prepare_default_values<EconomicFacilityCatalog>());EconomicFacilityCatalog out;
 facility_codec::checked(archive.defer_duplicate_field_check_to_finish());facility_codec::AuthoredDecodeContext authored{identity};facility_codec::Decoder decoder{archive,input,"/economic_facility_catalog",&authored};facility_codec::AuthoredCatalogFields<facility_codec::Decoder> fields{decoder};economic_facility_fields::fields(fields,out);decoder.finish();
 out.identity=copy_native_catalog_identity_for_decode(identity);return {true,std::move(out),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status emit_economic_facility_catalog(ProjectionWriter& w,const EconomicFacilityCatalog& v){return facility_codec::project(w,"",v);}
Status visit_economic_facility_catalog(CodecVisitor& c,EconomicFacilityCatalog& v){facility_codec::Checkpointer a{c};facility_codec::record(a,v);return c.status();}
BoundaryValue encode_production_facility_conditions(const ProductionFacilityConditionsState& v){return facility_codec::encode(v);}
Outcome<ProductionFacilityConditionsState> decode_production_facility_conditions(const BoundaryValue& v){try{auto out=facility_codec::decode_root<ProductionFacilityConditionsState>(v,"/production_facility_conditions");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status emit_production_facility_conditions(ProjectionWriter& w,const ProductionFacilityConditionsState& v){return facility_codec::project(w,"",v);}
Status visit_production_facility_conditions(CodecVisitor& c,ProductionFacilityConditionsState& v){facility_codec::Checkpointer a{c};facility_codec::record(a,v);return c.status();}
BoundaryValue encode_production_facility_conditions_catalog(const ProductionFacilityConditionsCatalog& v){return facility_codec::encode(v);}
Outcome<ProductionFacilityConditionsCatalog> decode_production_facility_conditions_catalog(const BoundaryValue& v){try{auto out=facility_codec::decode_root<ProductionFacilityConditionsCatalog>(v,"/production_facility_conditions_catalog");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 作者Condition只借已经解码的同目录09种子，不能把任意摘要当安装来源。
Outcome<ProductionFacilityConditionsCatalog> decode_authored_production_facility_conditions_catalog(const BoundaryValue& input,const CatalogIdentity& identity,const EconomicFacilityCatalog& facilities){try{
 auto valid=validate_economic_facility_catalog_identity(facilities,identity);if(!valid.ok)return {false,{},valid.error};
 ActorFoundationJsonVisitor archive(input);facility_codec::checked(archive.template prepare_default_values<ProductionFacilityConditionsCatalog>());ProductionFacilityConditionsCatalog out;
 facility_codec::AuthoredDecodeContext authored{identity,&facilities};facility_codec::checked(facility_codec::json_value(archive,input,out,"/production_facility_conditions_catalog",&authored));facility_codec::checked(archive.finish());
 return {true,std::move(out),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status emit_production_facility_conditions_catalog(ProjectionWriter& w,const ProductionFacilityConditionsCatalog& v){return facility_codec::project(w,"",v);}
Status visit_production_facility_conditions_catalog(CodecVisitor& c,ProductionFacilityConditionsCatalog& v){facility_codec::Checkpointer a{c};facility_codec::record(a,v);return c.status();}
Outcome<Integer> read_production_facility_condition_fact_payload_schema(const CommonStore&common,const ProductionFacilityConditionsState&state,const EconomicFacilitySourcesState&facilities,const FactRecord&head){
 if(facilities.schema_version!=1)return {false,0,common_failure("UnsupportedEconomicFacilitySourceSchema").error};
 const auto*actual=common.state().facts.find(head.id);const auto*change=state.changes.find(head.id);
 if(!actual||actual->id!=head.id||actual->domain!=head.domain||actual->kind!=head.kind||head.kind!="facility_condition_change"||!change||change->change_id!=head.id)return {false,0,common_failure("FacilityConditionOriginalPayloadIdentity",head.id).error};
 for(const auto&row:state.changes.rows())if(row.second.change_id==head.id&&row.first!=head.id)return {false,0,common_failure("FacilityConditionOriginalPayloadIdentity",head.id).error};
 const StableId*owner=nullptr;for(const auto&facility:facilities.facilities)if(facility.original_asset_ref==change->facility_ref){if(owner&&*owner!=facility.asset_owner_domain)return {false,0,common_failure("FacilityConditionOriginalOwnerAmbiguous",head.id).error};owner=&facility.asset_owner_domain;}
 if(!owner||(*owner!="local_economy"&&*owner!="estate"&&*owner!="commerce")||head.domain!=*owner)return {false,0,common_failure("FacilityConditionOriginalOwnerRequired",head.id).error};
 return {true,1,{}};
}
Status visit_production_facility_condition_fact_headers(const CommonStore&common,const ProductionFacilityConditionsState&state,const EconomicFacilitySourcesState&facilities,ProductionFacilityConditionFactHeaderSink&sink){
 for(const auto&row:state.changes.rows()){const auto*head=common.state().facts.find(row.first);if(!head)return common_failure("FacilityConditionOriginalHeaderRequired",row.first);auto schema=read_production_facility_condition_fact_payload_schema(common,state,facilities,*head);if(!schema.ok)return {false,{},schema.error};}
 for(const auto&row:common.state().facts.rows())if(row.second.kind=="facility_condition_change"&&(row.second.domain=="local_economy"||row.second.domain=="estate"||row.second.domain=="commerce")){if(row.first!=row.second.id)return common_failure("FacilityConditionOriginalHeaderIdentity",row.first);auto schema=read_production_facility_condition_fact_payload_schema(common,state,facilities,row.second);if(!schema.ok)return {false,{},schema.error};}
 for(const auto&row:common.state().facts.rows())if(row.second.kind=="facility_condition_change"&&(row.second.domain=="local_economy"||row.second.domain=="estate"||row.second.domain=="commerce")){auto appended=sink.append(row.second,1);if(!appended.ok)return appended;}
 return common_success();
}
Status emit_production_facility_condition_fact_payload_fields(ProjectionWriter&writer,const CommonStore&common,const ProductionFacilityConditionsState&state,const EconomicFacilitySourcesState&facilities,const FactRecord&head){
 auto schema=read_production_facility_condition_fact_payload_schema(common,state,facilities,head);if(!schema.ok)return {false,{},schema.error};
 facility_codec::Projector fields{writer};facility_codec::record(fields,const_cast<FacilityConditionChange&>(*state.changes.find(head.id)));return writer.status();
}

}

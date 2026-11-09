// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "private_wallet_title.hpp"
#include "resident/domains/estate/estate_model.hpp"
#include "resident/domains/family/family_model.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/domains/actor_foundation/created_origin.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{v.error};return std::move(v.value);}
std::size_t position(const CommonStore&c,const SourceRef&s){const auto*h=c.state().facts.find(s.id);const auto&order=c.state().facts.order();const auto found=std::find(order.begin(),order.end(),s.id);if(!h||h->id!=s.id||h->domain!=s.domain||h->kind!=s.kind||found==order.end())fail("PrivateWalletOriginalHeaderRequired",s.id);return static_cast<std::size_t>(found-order.begin());}
bool private_owner(const Owner&o){return !o.extra&&!o.id.empty()&&(o.kind==Owner::Kind::actor||o.kind==Owner::Kind::household);}
bool same_source(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool estate_wallet(const PrivateWalletTitleContext&x,const StableId&id){for(const auto&r:x.estates.estates.rows())if(r.second.wallet_id==id)return true;return false;}
PrivateWalletTitleView initial_title(const PrivateWalletTitleContext&x,const StableId&id,const SourceRef*cut){
 if(const auto*initial=x.catalog.required_wallets.find(id)){
  if(!private_owner(initial->owner))fail("PrivateWalletOriginalInitialTitleRequired",id);
  return {id,initial->owner,RequiredPrivateWalletOrigin{{"common_catalog","wallet",id}},{}};
 }
 if(!x.foundation||!x.foundation_catalog)fail("PrivateWalletAuthoredInitializationRequired",id);
 // This is the original native_actor_seed selection, derived from the locked
 // definitions. A personal: prefix or today's Owner is not initialization proof.
 const AuthoredActorFoundationDefinition*definition=nullptr;
 for(const auto&row:x.foundation_catalog->actors.rows()){
  if(id!="personal:"+row.first)continue;
  if(definition||row.second.actor_id!=row.first)fail("PrivateWalletAuthoredActorMismatch",id);
  definition=&row.second;
 }
 if(!definition){
  const CharacterOriginRecordV2*created=nullptr;
  for(const auto&row:x.foundation->character_origin_records.origins.rows())if(row.second.origin_kind==CharacterOriginRecordV2::Kind::created&&id=="personal:"+row.first){if(created||row.first!=row.second.actor_id)fail("PrivateWalletCreatedOriginMismatch",id);created=&row.second;}
  if(!created||!x.family_catalog||!x.clock||estate_wallet(x,id))fail("PrivateWalletOriginalInitialTitleRequired",id);
  const auto actual=take(read_created_character_origin_v2(x.common,*x.foundation,*x.foundation_catalog,*x.family_catalog,x.catalog,created->actor_id,take(x.clock->read_current_moment())));
  if(!actual.origin||actual.origin!=created||!actual.registration||!actual.creation)fail("PrivateWalletCreatedInitializationRequired",id);
  const SourceRef source{"family","actor_creation",created->actor_id};
  const auto at=position(x.common,source);if(cut&&at>=position(x.common,*cut))fail("PrivateWalletNotCreatedAtAdoption",id);
  return {id,{Owner::Kind::actor,created->actor_id,{}},CreatedPersonalWalletOrigin{source,2,created->actor_id,actual.registration->registration_result_id,created->created_at.exact_moment},{}};
 }
 const Owner initial_owner{Owner::Kind::actor,definition->actor_id,{}};
 const StableId*selected=nullptr;
 for(const auto&row:x.catalog.required_wallets.rows())if(!estate_wallet(x,row.first)&&owner_equal(row.second.owner,initial_owner)&&(!selected||row.first<*selected))selected=&row.first;
 // If an authored non-Estate wallet existed, the seed did not create personal:id.
 if(selected||estate_wallet(x,id))fail("PrivateWalletGeneratedSeedSelectionMismatch",id);
 auto origin=take(read_authored_character_origin_v2(x.common.state(),*x.foundation,*x.foundation_catalog,definition->actor_id));
 if(!origin.actor_start_context_ref||!actor_foundation_origin_equal(*origin.actor_start_context_ref,definition->scenario_origin_ref))fail("PrivateWalletAuthoredOriginMismatch",id);
 const auto&actual_origin=definition->scenario_origin_ref;
 if(actual_origin.initial_tick!=0||actual_origin.row_key.empty())fail("PrivateWalletAuthoredOriginMismatch",id);
 Integer retained=0;for(const auto&r:x.foundation->actor_start_context.scenario_origins)if(actor_foundation_origin_equal(r.ref,actual_origin))++retained;
 const ActorConstructionReceipt*construction=nullptr;
 for(const auto&row:x.foundation->actor_start_context.construction_receipts.rows()){
  const auto&receipt=row.second;const auto count=std::count(receipt.constructed_actor_ids.begin(),receipt.constructed_actor_ids.end(),definition->actor_id);if(!count)continue;
  if(count!=1||construction||!actor_foundation_origin_equal(receipt.scenario_origin_ref,actual_origin)||receipt.initialization_result_ref.empty()||row.first!=receipt.initialization_result_ref||!actor_foundation_rule_equal(receipt.construction_profile_ref,x.foundation_catalog->construction_profile))fail("PrivateWalletAuthoredConstructionMismatch",id);
  if(std::count_if(receipt.initial_source_refs.begin(),receipt.initial_source_refs.end(),[&](const auto&ref){return ref.seed_key==definition->condition.seed_key;})!=1)fail("PrivateWalletAuthoredConstructionSeedMismatch",id);
  construction=&receipt;
 }
 const auto*actor=x.common.actor(definition->actor_id);
 if(retained!=1||!construction||!actor||actor->wallet_id!=id)fail("PrivateWalletAuthoredInitializationRequired",id);
 return {id,initial_owner,AuthoredPersonalWalletOrigin{actual_origin,definition->actor_id,construction->initialization_result_ref},{}};
}
PrivateWalletTitleView read(const PrivateWalletTitleContext&x,const StableId&id,const SourceRef*cut){
 auto out=initial_title(x,id,cut);
 const auto end=cut?position(x.common,*cut):x.common.state().facts.size();const EstateDefinition*estate=nullptr;
 for(const auto&row:x.estates.estates.rows())if(row.second.wallet_id==id){if(estate)fail("PrivateWalletEstateAlias",id);estate=&row.second;}
 if(estate){if(!owner_equal(out.legal_owner,{Owner::Kind::actor,estate->owner_actor_id,{}}))fail("PrivateWalletEstateInitialTitleMismatch",id);auto actual=cut?take(x.estate_titles.read_estate_title_before_source(estate->id,*cut)):take(x.estate_titles.read_current_estate_title(estate->id));if(actual.wallet_id!=id||actual.estate_id!=estate->id)fail("PrivateWalletEstateTitleMismatch",id);out.legal_owner=actual.legal_owner;out.original_title_applications=std::move(actual.original_title_applications);}
 // The only original non-public SetWalletOwner route is the Estate succession.
 // Check both directions; missing original rows cannot certify unchanged title.
 for(const auto&header:x.common.state().facts.rows()){
  const auto&h=header.second;
  if(h.domain=="family"&&h.kind=="family_succession"){
   const auto*row=x.family.facts.find(h.id);const auto*f=row?std::get_if<FamilyChoice>(row):nullptr;if(!f||f->kind!="family_succession"||!f->assets||std::count(x.family.successions.begin(),x.family.successions.end(),h.id)!=1)fail("PrivateWalletSuccessionPayloadRequired",h.id);
   for(const auto&asset:f->assets->estates)if(asset.wallet_id==id){if(!estate||asset.estate_id!=estate->id||asset.inventory_id!=estate->inventory_id)fail("PrivateWalletOriginalTitleRouteRequired",id);const bool adopted=position(x.common,{h.domain,h.kind,h.id})<end;const auto matches=std::count_if(out.original_title_applications.begin(),out.original_title_applications.end(),[&](const auto&s){return same_source(s,{h.domain,h.kind,h.id});});if(adopted?matches!=1:matches!=0)fail("PrivateWalletTitleApplicationCoverage",h.id);}
  }
  if(h.domain=="military"&&h.kind=="military_occupation"){
   const MilitaryOccupationFact*found=nullptr;for(const auto&row:x.military.occupation_facts)if(row.id==h.id){if(found)fail("DuplicatePrivateWalletTitleSource",h.id);found=&row;}if(!found)fail("PrivateWalletOccupationPayloadRequired",h.id);
   // The original occupation writer is restricted to public assets. It cannot
   // be borrowed as an undocumented private-wallet transfer or spending grant.
   if(std::find(found->public_wallet_ids.begin(),found->public_wallet_ids.end(),id)!=found->public_wallet_ids.end())fail("PrivateWalletPublicOccupationConflict",id);
   if(found->asset_manifest)for(const auto&wallet:found->asset_manifest->wallets)if(wallet.first==id)fail("PrivateWalletPublicOccupationConflict",id);
  }
 }
 for(const auto&id:x.family.successions)position(x.common,{"family","family_succession",id});
 for(const auto&f:x.military.occupation_facts)position(x.common,{"military","military_occupation",f.id});
 if(!private_owner(out.legal_owner))fail("PrivateWalletTitleNotPrivate",id);
 if(!cut){const auto*w=x.common.wallet(id);if(!w||!owner_equal(w->owner,out.legal_owner))fail("PrivateWalletCurrentTitleMismatch",id);}
 return out;
}
}
Outcome<PrivateWalletTitleView> NativePrivateWalletTitleRead::read_current_private_wallet_title(const StableId&id)const{try{return {true,read(x_,id,nullptr),{}};}catch(const Failure&e){return {false,{},e.error};}}
Outcome<PrivateWalletTitleView> NativePrivateWalletTitleRead::read_private_wallet_title_before_source(const StableId&id,const SourceRef&cut)const{try{return {true,read(x_,id,&cut),{}};}catch(const Failure&e){return {false,{},e.error};}}
}

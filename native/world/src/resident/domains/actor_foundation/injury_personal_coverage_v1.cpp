#include "injury_personal_coverage_v1.hpp"
#include "injury_sources_v1.hpp"
#include "injury_original_results_v1.hpp"
#include "injury_source_links_v1.hpp"
#include "character_condition_read.hpp"
#include "resident/domains/personal/personal_battle_lifecycle.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
Status validate_personal_injury_source_coverage_v1(
 const CharacterConditionSources& body, const CharacterInjurySourcesStateV1& sources,
 const PersonalState& personal, const PersonalCatalog& catalog, Integer selected_lifecycle) {
 if(!catalog.enabled || selected_lifecycle!=1)
  return common_failure("CharacterInjuryPersonalOriginalSelectionRequired",{},ErrorKind::unsupported_capability);
 auto valid=validate_personal_battle_lifecycle_sources(body.common,personal,selected_lifecycle,body.actual_now);
 if(!valid.ok)return valid;
 valid=validate_character_injury_source_links_v1(body.common,body.state,body.catalog,sources);
 if(!valid.ok)return valid;
 // First authenticate all cited original rows, including incoming-zero writes.
 // The caller's source/root objects remain borrowed; there is no copied ledger.
 for(const auto& entry:sources.applications.rows()) {
  const auto* original=std::get_if<CharacterBattleReceiptSourceV1>(&entry.second.original_source);
  if(!original || (original->original_receipt.domain!="personal" &&
                   original->original_receipt.kind!="personal_battle"))continue;
  const auto* receipt=body.state.character_condition_extensions.applied_receipts.find(entry.first);
  if(!receipt)return common_failure("CharacterInjuryPersonalReceiptMissing",entry.first);
  valid=validate_personal_injury_original_result_v1(body,sources,*receipt,personal,catalog,selected_lifecycle);
  if(!valid.ok)return valid;
 }
 // Reverse scan actual results, not applications: omitting all source rows or
 // omitting an unwounded participant cannot make an incomplete state pass.
 for(const auto& entry:personal.facts.rows()) {
  const auto& fact=entry.second;
  if(entry.first!=fact.battle_id || fact.participants.empty())
   return common_failure("CharacterInjuryPersonalOriginalResultInvalid",entry.first);
  for(const auto& actor:fact.participants) {
   std::size_t matches=0;
   for(const auto& application:sources.applications.rows()) {
    const auto* original=std::get_if<CharacterBattleReceiptSourceV1>(&application.second.original_source);
    if(original && original->original_receipt.domain=="personal" &&
       original->original_receipt.kind=="personal_battle" &&
       original->original_receipt.id==entry.first && application.second.actor_id==actor)
     ++matches;
   }
   if(matches!=1)
    return common_failure("CharacterInjuryPersonalParticipantCoverageMismatch",entry.first);
  }
 }
 return common_success();
}
}

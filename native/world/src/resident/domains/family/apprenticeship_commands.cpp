// 报价与执行共用实际本人源和旧账；旧预览不能在另一世界代、角色或条款上生效。
#include "apprenticeship_commands.hpp"
#include "apprenticeship_receipts.hpp"
#include "succession_personal_assets.hpp"
#include <tuple>
namespace hanmo::resident {namespace {
bool id(const StableId& value){return !value.empty()&&value.size()<=256;}
bool shape(const ApprenticeshipCommand& c){
    if(c.kind=="family_apprenticeship_receive")return id(c.statement_id)&&c.proposal_id.empty()&&c.decision.empty()&&c.bond_id.empty();
    if(c.kind=="family_apprenticeship_reply")return c.statement_id.empty()&&id(c.proposal_id)&&(c.decision=="accepted"||c.decision=="refused"||c.decision=="withdrawn")&&c.bond_id.empty();
    return c.kind=="family_apprenticeship_exit"&&c.statement_id.empty()&&c.proposal_id.empty()&&c.decision.empty()&&id(c.bond_id);
}
bool same(const ApprenticeshipCommand& a,const ApprenticeshipCommand& b){return std::tie(a.kind,a.statement_id,a.proposal_id,a.decision,a.bond_id)==std::tie(b.kind,b.statement_id,b.proposal_id,b.decision,b.bond_id);}
bool identity(const WorldIdentity& a,const WorldIdentity& b){return std::tie(a.world_id,a.execution_epoch,a.revision,a.tick)==std::tie(b.world_id,b.execution_epoch,b.revision,b.tick);}
bool pair(const ApprenticeshipStatementSource& a,const ApprenticeshipStatementSource& b){return std::tie(a.teacher_actor_id,a.student_actor_id,a.skill_id)==std::tie(b.teacher_actor_id,b.student_actor_id,b.skill_id);}
}
Outcome<ApprenticeshipCommand> decode_apprenticeship_command(const StableId& kind,const BoundaryValue& input){try{
    using namespace boundary;auto unique=validate_succession_json_unique(input);if(!unique.ok)return {false,{},unique.error};ApprenticeshipCommand c;c.kind=kind;
    if(kind=="family_apprenticeship_receive"){fields(input,{"statement_id"},{},kind);c.statement_id=get_string(input,"statement_id","");}
    else if(kind=="family_apprenticeship_reply"){fields(input,{"proposal_id","decision"},{},kind);c.proposal_id=get_string(input,"proposal_id","");c.decision=get_string(input,"decision","");}
    else if(kind=="family_apprenticeship_exit"){fields(input,{"bond_id"},{},kind);c.bond_id=get_string(input,"bond_id","");}
    else fail("UnknownApprenticeshipCommand",kind);if(!shape(c))fail("InvalidApprenticeshipCommand",kind);return {true,std::move(c),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
BoundaryValue encode_apprenticeship_command(const ApprenticeshipCommand& c){using V=BoundaryValue;if(!shape(c))boundary::fail("InvalidApprenticeshipCommand",c.kind);if(c.kind=="family_apprenticeship_receive")return boundary::obj({{"statement_id",V{c.statement_id}}});if(c.kind=="family_apprenticeship_reply")return boundary::obj({{"proposal_id",V{c.proposal_id}},{"decision",V{c.decision}}});return boundary::obj({{"bond_id",V{c.bond_id}}});}
Outcome<ApprenticeshipQuote> preview_apprenticeship_command(const CommonStore& c,const InteractionState& meeting,const InteractionCatalog& interaction,const InteractionAccess& contact,const ApprenticeshipCatalog& catalog,const FamilyApprenticeshipState& state,const ApprenticeshipIdentityRead& access,const StableId& actor,const ApprenticeshipCommand& command){
    auto fail=[](const Status& s)->Outcome<ApprenticeshipQuote>{return {false,{},s.error};};
    if(!catalog.enabled)return fail(common_failure("ApprenticeshipProfileRequired"));if(!shape(command))return fail(common_failure("InvalidApprenticeshipCommand"));const auto* person=c.actor(actor);if(actor!=c.state().metadata.player_actor_id||!person||!person->available||!person->action_id.empty())return fail(common_failure("ApprenticeshipPersonalDecisionRequired"));
    auto history=validate_family_apprenticeship(c,interaction,catalog,state,access);if(!history.ok)return {false,{},history.error};auto valid=validate_apprenticeship_result_bindings(c,state);if(!valid.ok)return fail(valid);valid=validate_apprenticeship_person(c,*interaction.common,access,actor,c.tick());if(!valid.ok)return fail(valid);
    ApprenticeshipQuote quote{c.state().identity,command,{}};
    if(command.kind=="family_apprenticeship_exit"){const auto* bond=history.value.bonds.find(command.bond_id);if(!bond||bond->exit_fact_id||(actor!=bond->teacher_actor_id&&actor!=bond->student_actor_id))return fail(common_failure("ApprenticeshipExitAuthorityRequired"));return {true,std::move(quote),{}};}
    StableId statement=command.statement_id;
    if(command.kind=="family_apprenticeship_reply"){const auto* p=state.proposals.find(command.proposal_id);if(!p||history.value.terminal_proposals.count(command.proposal_id))return fail(common_failure("ApprenticeshipProposalNotPending"));if(actor!=(command.decision=="withdrawn"?p->source.speaker_actor_id:p->source.listener_actor_id))return fail(common_failure("ApprenticeshipReplyAuthorityRequired"));if(command.decision!="accepted")return {true,std::move(quote),{}};statement=p->source.statement_id;}
    auto source=preview_apprenticeship_statement(c,meeting,interaction,contact,catalog,access,actor,statement);if(!source.ok)return {false,{},source.error};quote.source=source.value;
    for(const auto& row:history.value.bonds.rows()){const auto& b=row.second;const auto& s=source.value.source;if(!b.exit_fact_id&&std::tie(b.teacher_actor_id,b.student_actor_id,b.skill_id)==std::tie(s.teacher_actor_id,s.student_actor_id,s.skill_id))return fail(common_failure("ApprenticeshipAlreadyActive"));}
    if(command.kind=="family_apprenticeship_receive"){for(const auto& row:state.proposals.rows())if(!history.value.terminal_proposals.count(row.first)&&pair(row.second.source,source.value.source))return fail(common_failure("ApprenticeshipProposalPending"));}
    else{const auto& old=state.proposals.find(command.proposal_id)->source;if(!pair(old,source.value.source)||old.speaker_actor_id!=source.value.source.speaker_actor_id||old.listener_actor_id!=source.value.source.listener_actor_id||old.terms_version!=source.value.source.terms_version)return fail(common_failure("ApprenticeshipConfirmationMismatch"));}
    return {true,std::move(quote),{}};
}
Outcome<FamilyEffect> apply_apprenticeship_command_in_candidate(CommonStore& c,const InteractionState& meeting,const InteractionCatalog& interaction,const InteractionAccess& contact,const ApprenticeshipCatalog& catalog,FamilyApprenticeshipState& state,const ApprenticeshipIdentityRead& access,const StableId& actor,const ApprenticeshipCommand& command,const ApprenticeshipQuote& quote){
    if(!same(command,quote.command)||!identity(c.state().identity,quote.identity))return {false,{},common_failure("StaleApprenticeshipQuote").error};auto fresh=preview_apprenticeship_command(c,meeting,interaction,contact,catalog,state,access,actor,command);if(!fresh.ok)return {false,{},fresh.error};if(bool(fresh.value.source)!=bool(quote.source)||(quote.source&&!apprenticeship_source_equal(fresh.value.source->source,quote.source->source)))return {false,{},common_failure("ApprenticeshipQuoteSourceChanged").error};
    Outcome<StableId> written;if(command.kind=="family_apprenticeship_receive")written=receive_apprenticeship_proposal_in_candidate(c,meeting,interaction,contact,catalog,state,access,*fresh.value.source);else if(command.kind=="family_apprenticeship_reply")written=reply_apprenticeship_proposal_in_candidate(c,meeting,interaction,contact,catalog,state,access,command.proposal_id,command.decision,fresh.value.source);else written=exit_apprenticeship_in_candidate(c,interaction,catalog,state,access,command.bond_id);if(!written.ok)return {false,{},written.error};
    FamilyEffect effect;effect.success=true;effect.actor_id=effect.player_actor_id=actor;effect.fact_id=written.value;return {true,std::move(effect),{}};
}
BoundaryValue encode_apprenticeship_quote(const ApprenticeshipQuote& quote){using V=BoundaryValue;V::Dict rows{{"ok",V{true}},{"kind",V{quote.command.kind}},{"params",encode_apprenticeship_command(quote.command)},{"duration",V{Integer{0}}},{"money",V{Integer{0}}},{"scope",V{"personal_relation_only"}}};if(quote.source){rows.emplace_back("teacher_actor_id",V{quote.source->source.teacher_actor_id});rows.emplace_back("student_actor_id",V{quote.source->source.student_actor_id});rows.emplace_back("skill_id",V{quote.source->source.skill_id});}return V{std::move(rows)};}
}

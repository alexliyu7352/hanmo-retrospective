#include "visit_message_receipt_world.hpp"
// 所有借用在原预览/提交栈内；缓存回执仅复用旧InteractionEffect的具名事实字段。
#include "visit_message_world.hpp"
#include "visit_message_native.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <set>
namespace hanmo::resident {namespace {
Status enabled(const WorldBundle& w){return w.dynamic_enabled()&&w.catalog->interaction&&w.catalog->interaction->visit_messages&&w.interaction.visit_messages?common_success():common_failure("VisitMessageProfileRequired");}
struct Read {std::unique_ptr<CompleteExactWorldBindingOwner>complete;std::unique_ptr<NativeVisitMessageLifeRead>native;const VisitMessageLifeContext*life=nullptr;};
Outcome<Read>read(const WorldBundle&w,const CompleteExactWorldReadBindings*supplied){Read out;if(w.interaction.visit_messages->version==1)return {true,std::move(out),{}};const auto*b=supplied;if(!b){auto lease=w.complete_exact_time_bindings();if(!lease.ok)return {false,{},lease.error};out.complete=std::move(lease.value);if(!out.complete)return {false,{},common_failure("VisitMessageCompleteOwnerRequired",{},ErrorKind::unsupported_capability).error};b=&out.complete->reads();}auto native=NativeVisitMessageLifeRead::create(w,b->character_conditions);if(!native.ok)return {false,{},native.error};out.native=std::move(native.value);auto context=out.native->context();if(!context.ok)return {false,{},context.error};out.life=context.value;return {true,std::move(out),{}};}
bool fact_id(const StableId& id){return id.rfind("visit_message_posted:",0)==0||id.rfind("visit_message_received:",0)==0||id.rfind("visit_message_withdrawn:",0)==0;}
}
bool visit_message_command_kind(const StableId& kind){return kind=="leave_visit_message"||kind=="receive_visit_message"||kind=="withdraw_visit_message";}
Status validate_visit_message_world(const WorldBundle& w,const CompleteExactWorldReadBindings*actual){
    const auto* catalog=w.catalog->interaction?w.catalog->interaction->visit_messages.get():nullptr;const auto* state=w.interaction.visit_messages?&*w.interaction.visit_messages:nullptr;
    auto valid=validate_visit_message_layout(w.common,w.catalog->identity.module_versions,catalog,state);if(!valid.ok)return valid;
    if(catalog){valid=enabled(w);if(!valid.ok)return valid;auto source=read(w,actual);if(!source.ok)return {false,{},source.error};valid=validate_visit_messages(w.common,*state,*catalog,*w.catalog->interaction,source.value.life?&source.value.life->clock:nullptr);if(!valid.ok)return valid;if(source.value.life){valid=validate_visit_message_life_sources(*source.value.life);if(!valid.ok)return valid;}}
    for(const auto& history:w.common.state().history)if(visit_message_command_kind(history.kind)&&!state)return common_failure("VisitMessageProfileRequired",history.result_id);
    WorldCurrentMomentRead clock(w);std::set<StableId> used;
    for(const auto& row:w.common.state().results.rows())if(row.second){const auto& result=*row.second;const auto* receipt=result.details?std::get_if<InteractionEffect>(&result.details->data):nullptr;
        const bool command=visit_message_command_kind(result.kind),message=receipt&&fact_id(receipt->settlement_result);if(!command&&!message)continue;
        if(!state||!command||!message||!receipt->encounter_id.empty()||receipt->party_id||receipt->person_id||receipt->news_ids)return common_failure("InvalidVisitMessageReceipt",row.first);
        if(!used.insert(receipt->settlement_result).second)return common_failure("DuplicateVisitMessageCachedSource",row.first);
        valid=validate_visit_message_cached_source(w.common,*state,row.first,result.kind,result.result_id,result.tick,receipt->settlement_result,state->version>=2?&clock:nullptr);if(!valid.ok)return valid;
    }
    return common_success();
}
Outcome<VisitMessageQuote> prepare_visit_message_world(const WorldBundle& w,const StableId& actor,const VisitMessageCommand& command,const CompleteExactWorldReadBindings*actual){auto ready=enabled(w);if(!ready.ok)return {false,{},ready.error};auto source=read(w,actual);if(!source.ok)return {false,{},source.error};const auto*bindings=actual?actual:(source.value.complete?&source.value.complete->reads():nullptr);auto producer=guard_visit_message_receipt_producer_world(w,actor,command,bindings);if(!producer.ok)return {false,{},producer.error};auto access=w.services.interaction_access();return preview_visit_message(w.common,*w.interaction.visit_messages,*w.catalog->interaction->visit_messages,w.interaction,*w.catalog->interaction,access,actor,command,source.value.life);}
Outcome<InteractionEffect> commit_visit_message_world(WorldBundle& w,const StableId& actor,const VisitMessageCommand& command,const VisitMessageQuote& quote,const CompleteExactWorldReadBindings*actual){auto ready=enabled(w);if(!ready.ok)return {false,{},ready.error};auto source=read(w,actual);if(!source.ok)return {false,{},source.error};const auto*bindings=actual?actual:(source.value.complete?&source.value.complete->reads():nullptr);auto producer=guard_visit_message_receipt_producer_world(w,actor,command,bindings);if(!producer.ok)return {false,{},producer.error};auto access=w.services.interaction_access();auto applied=apply_visit_message_in_candidate(w.common,*w.interaction.visit_messages,*w.catalog->interaction->visit_messages,w.interaction,*w.catalog->interaction,access,actor,command,quote,source.value.life);if(!applied.ok)return {false,{},applied.error};InteractionEffect effect;effect.settlement_result=applied.value;return {true,std::move(effect),{}};}
Outcome<BoundaryValue> query_visit_message_world(const WorldBundle& w,Integer cursor,Integer offer_cursor,const CompleteExactWorldReadBindings*actual){auto ready=enabled(w);if(!ready.ok)return {false,{},ready.error};auto source=read(w,actual);if(!source.ok)return {false,{},source.error};auto access=w.services.interaction_access();return query_visit_messages(w.common,*w.interaction.visit_messages,*w.catalog->interaction->visit_messages,w.interaction,*w.catalog->interaction,access,cursor,offer_cursor,source.value.life);}
BoundaryValue encode_visit_message_quote(const VisitMessageQuote& quote){using V=BoundaryValue;return boundary::obj({{"duration",V{Integer{0}}},{"policy_id",V{quote.policy_id}},{"message_fact_id",V{quote.message_fact_id}},{"already_completed",V{!quote.existing_result_fact_id.empty()}}});}
Status apply_visit_message_death_world_in_candidate(WorldBundle&w,const DeathRef&death,const CompleteExactWorldReadBindings*actual){auto valid=enabled(w);if(!valid.ok)return valid;auto source=read(w,actual);if(!source.ok)return {false,{},source.error};if(!source.value.life)return common_failure("VisitMessageLife4OwningSourcesRequired",{},ErrorKind::unsupported_capability);return apply_visit_message_death_in_candidate(w.common,*w.interaction.visit_messages,*source.value.life,death);}

}

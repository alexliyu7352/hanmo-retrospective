// 沿原场景同地可见人物规则先筛现场；出生/亲缘读仅在筛后用于本人可开的模板。
#include "born_personal_dialogue_view.hpp"
#include "born_personal_dialogue_detail.hpp"
#include "resident/views/common_presentation.hpp"
#include <limits>
namespace hanmo::resident {namespace {
using V=BoundaryValue;using A=V::Array;
V identity(const WorldIdentity& s){StableId id;constexpr char hex[]="0123456789abcdef";for(auto b:s.world_id){id+=hex[b>>4];id+=hex[b&15];}return boundary::obj({{"world_id",V{id}},{"execution_epoch",V{s.execution_epoch}},{"revision",V{s.revision}},{"tick",V{s.tick}}});}
StableId at(const InteractionAccess& a,const StableId& id){auto where=a.resolve_location(id);const auto* place=std::get_if<SettlementLocation>(&where);return place?place->place_id:StableId{};}
}
Outcome<BoundaryValue> query_born_personal_dialogues(const CommonStore& common,const InteractionState& interactions,const BornPersonalDialogueState& state,const InteractionCatalog& definitions,const BornPersonalDialogueCatalog& catalog,const InteractionAccess& access,const BornPersonalDialogueSources& sources,Integer cursor){
    constexpr Integer limit=16;const auto& actor=common.state().metadata.player_actor_id;if(cursor<0||cursor>std::numeric_limits<Integer>::max()-limit)return {false,{},common_failure("InvalidBornPersonalDialogueCursor").error};if(!common.actor(actor)||!access.resolve_location||!sources.born)return {false,{},common_failure("BornPersonalDialogueSourceRequired").error};
    auto valid=validate_born_personal_dialogues(common,interactions,state,definitions,catalog,sources);if(!valid.ok)return {false,{},valid.error};valid=validate_born_personal_dialogue_cached_results(common,&state);if(!valid.ok)return {false,{},valid.error};A all;const auto here=at(access,actor);CommonPresentation names(common.state(),*definitions.common);auto ids=common.actor_order();std::sort(ids.begin(),ids.end());
    if(!here.empty())for(const auto& id:ids){const auto* person=common.actor(id);if(!person||id==actor||!person->alive||at(access,id)!=here)continue;auto born=sources.born(id);if(!born.ok)return {false,{},born.error};if(!born.value||!born.value->adult||!born.value->alive||!born_personal_detail::is_parent(*born.value,actor))continue;
        for(const auto& t:catalog.templates){if(t.prototype_id!=born.value->prototype_id)continue;auto name=names.display_name(id);if(!name.ok)return {false,{},name.error};OpenBornPersonalDialogue request{id,t.id};auto quote=preview_born_personal_dialogue(common,interactions,state,definitions,catalog,access,sources,actor,request);if(!quote.ok&&quote.error.kind!=ErrorKind::rule)return {false,{},quote.error};A actions;actions.push_back(boundary::obj({{"command",V{"open_born_personal_dialogue"}},{"params",encode_born_personal_dialogue_command(request)},{"text",V{"与本人谈一谈"}},{"summary",V{"现在与眼前本人开始私人交谈。后续接受或撤回须到实际本人说出的独立节点，再由原规则办理。"}},{"available",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}}}));all.push_back(boundary::obj({{"id",V{t.id+"|"+id}},{"display_name",V{name.value}},{"status_text",V{"本人正在此处"}},{"progress_text",V{"只开始交谈，不自动答允、改亲缘、移交资产或控制权。"}},{"actions",V{std::move(actions)}}}));}
    }
    A rows;const auto end=std::min(all.size(),static_cast<std::size_t>(cursor+limit));for(std::size_t i=static_cast<std::size_t>(cursor);i<end;++i)rows.push_back(std::move(all[i]));V::Dict page{{"ok",V{true}},{"source_identity",identity(common.state().identity)},{"observer_actor_id",V{actor}},{"cursor",V{cursor}},{"rows",V{std::move(rows)}}};if(end<all.size())page.emplace_back("next_cursor",V{cursor+limit});return {true,V{std::move(page)}, {}};
}
}

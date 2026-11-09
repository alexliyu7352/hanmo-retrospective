// 双签只产生具名政治事实；旧外交、任务与钱货均由原业务独立维护。
#include "treaty_passage_integration.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
TreatyPassageWorldAccess::TreatyPassageWorldAccess(const WorldBundle& w,std::unique_ptr<PoliticalTakeoverWorldAccess> takeover):takeover_(std::move(takeover)),interaction_(w.services.interaction_access()),present_{w.common,w.organization,*w.catalog->organization,*w.catalog->diplomacy,w.diplomacy,w.interaction,*w.catalog->interaction,interaction_},missions_(w.common,w.organization,*w.catalog->organization,*w.catalog->diplomacy,w.diplomacy),territory_(w.common,w.politics,*w.catalog->politics,takeover_?&takeover_->context():nullptr),context_{present_,*w.catalog->treaty_passage,w.treaty_passage,territory_,nullptr}{
 if(w.diplomacy.version==2){reports_.emplace(present_,*w.diplomacy.v2);envoy_=std::make_unique<EnvoyTaskRead>(w.common,w.tasks,w.envoy_tasks,missions_,missions_,*reports_);}else envoy_=std::make_unique<EnvoyTaskRead>(w.common,w.tasks,w.envoy_tasks,missions_,missions_);context_.envoy_tasks=envoy_.get();
}
Outcome<std::unique_ptr<TreatyPassageWorldAccess>> TreatyPassageWorldAccess::create(const WorldBundle& w){
 if(!w.treaty_passage_enabled()||!w.treaty_passage_seed_complete||!w.politics_enabled()||!w.diplomacy_enabled()||!w.catalog->organization||!w.catalog->interaction||!w.catalog->diplomacy||(w.diplomacy.version==2&&!w.diplomacy.v2))return {false,{},common_failure("TreatyPassageProfileRequired").error};
 std::unique_ptr<PoliticalTakeoverWorldAccess> takeover;if(w.political_takeover_enabled()){auto read=PoliticalTakeoverWorldAccess::create(w,*w.catalog->political_takeover,w.political_takeover);if(!read.ok)return {false,{},read.error};takeover=std::move(read.value);}
 return {true,std::unique_ptr<TreatyPassageWorldAccess>(new TreatyPassageWorldAccess(w,std::move(takeover))),{}};
}
Status validate_treaty_passage_world(const WorldBundle& w){
 const bool enabled=w.treaty_passage_enabled();auto layout=validate_treaty_passage_layout(enabled,w.catalog->identity.module_versions,w.treaty_passage_seed_complete?&w.treaty_passage:nullptr);if(!layout.ok)return layout;auto history=validate_treaty_passage_command_history(w.common,enabled);if(!history.ok)return history;
 if(!enabled)return w.treaty_passage.deliveries.empty()&&w.treaty_passage.signatures.empty()?common_success():common_failure("UnexpectedTreatyPassageState");
 auto access=TreatyPassageWorldAccess::create(w);if(!access.ok)return {false,{},access.error};auto state=validate_treaty_passage_state(access.value->context());if(!state.ok)return state;return validate_treaty_passage_receipt_sources(access.value->context(),[](const CommitResult& result){return result.details&&std::holds_alternative<Done>(result.details->data);});
}
Outcome<TreatyPassageQuote> prepare_treaty_passage_world(const WorldBundle& w,const StableId& actor,const TreatyPassageCommand& command){auto access=TreatyPassageWorldAccess::create(w);if(!access.ok)return {false,{},access.error};return preview_treaty_passage_command(access.value->context(),actor,command);}
Outcome<StableId> commit_treaty_passage_world(WorldBundle& w,const StableId& actor,const TreatyPassageCommand& command,const TreatyPassageQuote& quote){auto access=TreatyPassageWorldAccess::create(w);if(!access.ok)return {false,{},access.error};return apply_treaty_passage_command_in_transaction(w.common,w.treaty_passage,access.value->context(),actor,command,quote);}
Outcome<bool> treaty_passage_world_admission(const WorldBundle& w,const StableId& actor,const StableId& place,const StableId& terms){auto access=TreatyPassageWorldAccess::create(w);if(!access.ok)return {false,false,access.error};return TreatyPassageAccess(access.value->context(),w.services).treaty_personal_passage(actor,place,terms);}
Outcome<BoundaryValue> merge_treaty_passage_world_page(const WorldBundle& w,const StableId& actor,BoundaryValue page){if(!w.treaty_passage_enabled())return {true,std::move(page),{}};auto access=TreatyPassageWorldAccess::create(w);if(!access.ok)return {false,{},access.error};return merge_treaty_passage_page(access.value->context(),*w.catalog->common,actor,std::move(page));}
}

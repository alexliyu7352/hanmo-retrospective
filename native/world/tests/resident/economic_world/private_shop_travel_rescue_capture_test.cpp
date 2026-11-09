// 原救助改种观察只留被真实替换的原载荷；不靠generic erase推断到达或原货损。
#define main private_shop_original_travel_boundary_main
#include "private_shop_travel_expense_source_test.cpp"
#undef main
#include "resident/domains/travel/travel_rescue_capture.hpp"
#include "resident/storage/common_codec.hpp"
namespace {
// 实际起行再耗尽原本人钱包，经过原救助资格后才改种，不手填rescue状态。
void original_rescue_snapshot(){
    Fixture f;auto id=f.begin("carrier","B");require(step_travel(f.common,f.travel,f.catalog,f.services,id).ok,"original paid step before stranding");
    require(f.common.apply_resources({{WalletDelta{"wallet.carrier",-93,true}},{}}).ok,"independent actual expenditure empties payer");require(preview_rescue(f.common,f.travel,f.catalog,f.services,"carrier").ok,"original stranded rescue preview");
    const auto before=*f.common.action(id);const auto params=*f.travel.road_actions.find(id);const auto next=f.common.state().next_instance_id;std::optional<TravelRescueReplacementDraft> draft;
    require(begin_rescue(f.common,f.travel,"carrier",&draft).ok&&draft.has_value(),"actual rescue replacement returns original snapshot");
    check(draft->original_action.id==id&&draft->original_action.kind=="travel"&&draft->original_action.start_tick==before.start_tick&&draft->original_action.remaining_ticks==before.remaining_ticks,"snapshot retains actual old action");
    const auto* old=std::get_if<RoadTravelParams>(&draft->original_travel);check(old&&old->path==params.path&&old->edge_index==params.edge_index&&old->payer_wallet_id==params.payer_wallet_id,"snapshot owns actual removed road parameters");
    check(!f.travel.road_actions.find(id)&&f.travel.rescues.find(id)&&f.common.action(id)->kind=="rescue"&&f.common.action(id)->remaining_ticks==2,"original writer changes kind and payload once");
    check(draft->replacement_action.id==id&&draft->replacement_action.kind=="rescue"&&draft->rescue.origin=="A"&&std::holds_alternative<OnRouteLocation>(draft->original_location),"rescue request is not arrival");
    check(f.funds("carrier")==0&&f.common.state().next_instance_id==next,"rescue capture neither charges nor allocates ID");
    auto twice=begin_rescue(f.common,f.travel,"carrier",&draft);check(!twice.ok&&!draft,"repeat non-travel failure clears old snapshot");
}
// 只有显式捕获要求唯一原载荷；缺失或双载荷在任何Action改种前拒绝。
void missing_or_ambiguous_original(){
    for(bool both:{false,true}){Fixture f;auto id=f.begin("carrier","B");if(both)f.travel.actions.insert(id,TerrainTravelParams{});else f.travel.road_actions.erase(id);
        auto before=encode_common_state(f.common.state());std::optional<TravelRescueReplacementDraft> draft;auto result=begin_rescue(f.common,f.travel,"carrier",&draft);
        check(!result.ok&&result.error.original.code=="TravelRescueOriginalPayloadRequired"&&!draft,"missing or ambiguous original refuses capture");check(compare_boundary(before,encode_common_state(f.common.state())).exact()&&!f.travel.rescues.find(id),"source refusal precedes original mutation");}
}
// 原两个半日减步后才完成救助，逐批实损/返城只在成功出口可见；有无观察的原银行逐字相等。
void actual_loss_and_return(){
    Fixture captured,legacy;
    for(auto* f:{&captured,&legacy}){
        f->common_catalog->items.insert("grain",{"grain",1,1});
        require(f->common.apply_resources({{AddLot{"bag.carrier","grain",21,{Owner::Kind::actor,"carrier",{}},{}},AddLot{"bag.carrier","grain",9,{Owner::Kind::actor,"carrier",{}},{}},AddLot{"bag.carrier","grain",19,{Owner::Kind::actor,"other",{}},{}}},{}}).ok,"actual original lots with distinct owners");
        auto id=f->begin("carrier","B");require(step_travel(f->common,f->travel,f->catalog,f->services,id).ok,"actual movement before rescue completion");
        require(f->common.apply_resources({{WalletDelta{"wallet.carrier",-f->funds("carrier"),true}},{}}).ok,"actual wallet expense before rescue");require(begin_rescue(f->common,f->travel,"carrier").ok,"original rescue replacement");
        for(int n=0;n<2;++n){auto current=*f->common.action(id);ActionMutation step;step.action=current;step.remaining_ticks=current.remaining_ticks-1;require(f->common.apply_action(step).ok,"original rescue half-day decrement");if(n==0)++f->common.state().identity.tick;}
    }
    const auto id=captured.common.actor("carrier")->action_id;const auto before=encode_common_state(captured.common.state());const auto allocation=captured.common.state().next_instance_id;std::optional<TravelRescueCompletionDraft> done;
    require(finish_rescue(captured.common,captured.travel,captured.catalog,id,&done).ok&&done.has_value(),"actual original loss and relocation captured");require(finish_rescue(legacy.common,legacy.travel,legacy.catalog,id).ok,"legacy rescue completion succeeds");
    check(compare_boundary(encode_common_state(captured.common.state()),encode_common_state(legacy.common.state())).exact(),"with and without capture original Common bytes equal");
    check(done->action.id==id&&done->action.kind=="rescue"&&done->action.remaining_ticks==0&&done->source_tick==1,"actual completed rescue action before release");
    check(done->before_inventory.lots.size()==3&&done->after_inventory.lots.size()==3&&done->applied_adjustment.lots.size()==2,"captures actual personal lot recipe not foreign ownership");
    check(done->applied_adjustment.lots[0].delta==-2&&done->applied_adjustment.lots[1].delta==0,"actual per-lot flooring keeps zero leg distinct");
    check(done->after_inventory.lots[0].quantity==19&&done->after_inventory.lots[1].quantity==9&&done->after_inventory.lots[2].quantity==19,"actual personal loss and untouched foreign batch");
    check(done->after_inventory.revision==done->before_inventory.revision+1&&captured.common.state().next_instance_id==allocation&&captured.funds("carrier")==0,"rescue loss one original revision no fee refund or ID");
    check(std::holds_alternative<OnRouteLocation>(done->before_location)&&std::get<SettlementLocation>(done->after_location).settlement_id=="A","actual return origin differs from original destination");
    check(captured.common.action(id)&&captured.travel.rescues.find(id),"completion capture does not replace central release");
    auto missing=finish_rescue(captured.common,captured.travel,captured.catalog,"missing.action",&done);check(!missing.ok&&!done,"completion failure clears caller prior snapshot");
}
// 显式完成捕获不接受尚未到期的原rescue行动，不提前扣货或回城。
void premature_completion_refuses(){
    Fixture f;auto id=f.begin("carrier","B");require(begin_rescue(f.common,f.travel,"carrier").ok,"original rescue request for boundary fixture");auto before=encode_common_state(f.common.state());std::optional<TravelRescueCompletionDraft> done;
    auto result=finish_rescue(f.common,f.travel,f.catalog,id,&done);check(!result.ok&&result.error.original.code=="TravelRescueCompletionBoundaryRequired"&&!done,"unfinished rescue cannot emit completion");check(compare_boundary(before,encode_common_state(f.common.state())).exact(),"premature completion has no resource or position writes");
}

}
// 必要源码夹具，运行身份须另行固定；本片无World或永久终结账。
int main(){try{original_rescue_snapshot();missing_or_ambiguous_original();actual_loss_and_return();premature_completion_refuses();}catch(const std::exception& e){check(false,e.what());}std::cout<<"TRAVEL_RESCUE_CAPTURE_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}

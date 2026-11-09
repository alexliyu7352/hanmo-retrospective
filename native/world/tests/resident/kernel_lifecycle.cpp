// 真实1000人种子的一次半日验证：许可幂等、取消、冷恢复旧epoch隔离与失败不发布。
#include "hanmo/resident_world_api.hpp"
#include "resident/kernel/world_codec.hpp"
#include "resident/kernel/command_types.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
void check(bool ok,const char* text){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<text<<'\n';}}
BoundaryValue read(const std::string& p){std::ifstream file(p);std::ostringstream text;text<<file.rdbuf();auto v=parse_boundary_json(text.str());if(!v.ok)throw std::runtime_error(v.error.original.code+":"+p);return std::move(v.value);}
// 直接调用typed入口；冷decoder的正确分派不能替代共同网关的kind/variant绑定。
void common_travel_bindings(ResidentWorld& world,const std::shared_ptr<const PreparedCatalog>& catalog,const CommandEnvelope& source){
    const auto before=world.checkpoint();check(before.ok,"binding fixture checkpoint");if(!before.ok)return;
    const std::vector<CommandPayload> commands={
        {"advance",AdvanceCommand{}},{"wait",WaitCommand{}},*source.payload,
        {"pause_travel",TravelCommandPayload{PauseTravelCommand{}}},
        {"resume_travel",TravelCommandPayload{ResumeTravelCommand{}}},
        {"travel_wait",TravelCommandPayload{TravelWaitCommand{}}},
        {"reroute",TravelCommandPayload{RerouteCommand{StableId{}, {}}}},
        {"return",TravelCommandPayload{ReturnTravelCommand{}}},
        {"rescue",TravelCommandPayload{RescueCommand{}}},
        {"move_local",TravelCommandPayload{MoveLocalCommand{}}}
    };
    for(const auto& typed:commands){
        auto request=source;request.payload=std::make_shared<const CommandPayload>(typed);
        auto canonical=world.preview(request);check(canonical.ok||canonical.error.original.code!="InvalidCommand","canonical basic kind reaches its original rule");
        for(const auto& outer:commands){if(outer.kind==typed.kind)continue;auto mismatched=typed;mismatched.kind=outer.kind;request.payload=std::make_shared<const CommandPayload>(std::move(mismatched));auto preview=world.preview(request);check(!preview.ok&&preview.error.original.code=="InvalidCommand","crossed basic kind/variant rejected before rules");}
        auto unknown=typed;unknown.kind="unknown_basic_command";request.payload=std::make_shared<const CommandPayload>(std::move(unknown));auto preview=world.preview(request);check(!preview.ok&&preview.error.original.code=="InvalidCommand","unknown outer kind cannot borrow a basic handler");
        request.payload=std::make_shared<const CommandPayload>(CommandPayload{typed.kind,TaskCommand{}});preview=world.preview(request);check(!preview.ok&&preview.error.original.code=="InvalidCommand","basic outer kind cannot borrow another domain payload");
    }
    check(world.checkpoint().value==before.value,"binding previews leave complete checkpoint unchanged");
    // 找实际可达地点后，在独立同核恢复实例检查：移动不能伪称wait并提交成功。
    StableId destination;
    for(const auto& place:catalog->travel->places.rows()){auto request=source;request.payload=std::make_shared<const CommandPayload>(CommandPayload{"move_local",TravelCommandPayload{MoveLocalCommand{place.first}}});if(world.preview(request).ok){destination=place.first;break;}}
    check(!destination.empty(),"real fixture has a legal local move");if(destination.empty())return;
    ResidentWorld probe;auto restored=probe.restore(before.value,catalog);check(restored.ok,"isolated binding submit fixture restores");if(!restored.ok)return;
    auto request=source;const auto identity=probe.identity().value;request.world_id=identity.world_id;request.execution_epoch=identity.execution_epoch;request.expected_revision=identity.revision;request.payload=std::make_shared<const CommandPayload>(CommandPayload{"wait",TravelCommandPayload{MoveLocalCommand{destination}}});
    auto rejected=probe.submit(request);check(!rejected.ok&&rejected.error.original.code=="InvalidCommand","mismatched typed submit rejects instead of moving as wait");
    const auto after=probe.checkpoint();check(after.ok&&after.value==before.value,"mismatched submit preserves complete world and request cache");
    // 内部NPC也复用同一个prepare/apply；具名PreparedOp不能借另一基础名字进入写路由。
    auto seed=decode_world_checkpoint(before.value,*catalog);check(seed.ok,"internal binding fixture decodes");if(!seed.ok)return;
    WorldBundle candidate(catalog,std::move(seed.value));auto prepared=prepare_command(candidate,source.actor_id,{"move_local",TravelCommandPayload{MoveLocalCommand{destination}}});check(prepared.ok,"same internal rule prepares actual local move");if(!prepared.ok)return;
    auto malformed=*prepared.value->operation;malformed.command.kind="wait";auto applied=apply_command(candidate,source.actor_id,malformed);check(!applied.ok&&applied.error.original.code=="InvalidCommand","internal apply also rejects crossed basic kind");
    auto unchanged=encode_world_checkpoint(candidate);check(unchanged.ok&&unchanged.value==before.value,"rejected internal apply leaves complete candidate unchanged");
}
}
int main(int argc,char** argv){if(argc!=2)return 2;const std::string root=argv[1];auto index=read(root+"/oracle-index.json");auto catalog=read(root+"/expanded-catalog.json");auto state=read(root+"/"+*index.find("initial_state")->find("file")->as<StableId>());
    auto prepared=prepare_catalog(catalog,state);check(prepared.ok,"prepare real catalog");if(!prepared.ok)return 1;auto seed=decode_world_seed(state,*prepared.value);check(seed.ok,"decode real seed");if(!seed.ok)return 1;
    ResidentWorld world;check(world.install(prepared.value,std::move(seed.value)).ok,"one typed world install");auto initial=world.identity();check(initial.ok,"identity");if(!initial.ok)return 1;
    auto command=decode_resident_command("travel",*index.find("commands")->as<BoundaryValue::Array>()->front().find("command")->find("params"));check(command.ok,"decode real travel");
    CommandEnvelope envelope;envelope.world_id=initial.value.world_id;envelope.execution_epoch=initial.value.execution_epoch;envelope.actor_id="actor.player";envelope.request_seq=1;envelope.expected_revision=0;envelope.payload=std::make_shared<const CommandPayload>(command.value);
    common_travel_bindings(world,prepared.value,envelope);
    auto preview_before=world.checkpoint();auto quote=world.preview(envelope);check(quote.ok,"valid typed travel preview");if(!quote.ok)return 1;auto owned_quote=encode_resident_preview(quote,initial.value.revision);check(owned_quote.ok&&world.checkpoint().value==preview_before.value,"valid preview is pure and encodes owned quote");
    auto started=world.submit(envelope);check(started.ok,"submit actual travel");if(!started.ok){std::cerr<<started.error.original.code<<'\n';return 1;}auto action=std::get<ActionStarted>(started.value->details->data).action_id;
    auto checkpoint=world.checkpoint();check(checkpoint.ok,"checkpoint before invalid command");
    auto old_quote=encode_resident_preview(quote,initial.value.revision);check(old_quote.ok&&owned_quote.ok&&compare_boundary(owned_quote.value,old_quote.value).exact(),"held preview stays unchanged after publish");
    auto retry=envelope;retry.expected_revision=-1;retry.payload=std::make_shared<const CommandPayload>(CommandPayload{"unknown_basic_command",WaitCommand{}});auto replayed=world.submit(retry);check(replayed.ok&&replayed.value==started.value,"successful same-seq replay precedes changed payload binding and revision");check(world.checkpoint().value==checkpoint.value,"changed-payload replay never writes");
    ResidentWorld replacement;check(replacement.restore(checkpoint.value,prepared.value).ok,"new instance restores same logical world");auto replacement_id=replacement.identity().value;check(replacement_id.world_id==initial.value.world_id&&replacement_id.execution_epoch!=initial.value.execution_epoch,"new instance does not reuse execution epoch");auto old_envelope=envelope;old_envelope.request_seq=2;old_envelope.expected_revision=1;auto old_rejected=replacement.submit(old_envelope);check(!old_rejected.ok&&old_rejected.error.original.code=="StaleWorld","old envelope rejected by replacement");auto old_poll=replacement.poll_run({initial.value.world_id,initial.value.execution_epoch,"run:test"});auto old_cancel=replacement.cancel_run({initial.value.world_id,initial.value.execution_epoch,"run:test"});check(!old_poll.ok&&old_poll.error.original.code=="StaleWorld"&&!old_cancel.ok&&old_cancel.error.original.code=="StaleWorld","old run refs rejected before replacement lookup");auto bad=envelope;bad.request_seq=2;bad.expected_revision=0;auto rejected=world.submit(bad);check(!rejected.ok&&rejected.error.original.code=="StalePreview","stale command rejected");auto unchanged=world.checkpoint();check(unchanged.ok&&unchanged.value==checkpoint.value,"failed submit exact bytes unchanged");
    auto id=world.identity().value;BeginRun request{id,"run:test","actor.player",action,4,100000};auto parked=world.begin_run(request);check(parked.ok&&parked.value.stop==RunStop::awaiting_permit,"begin parks only");check(world.identity().value.tick==0,"begin no hidden advance");
    auto polled=world.poll_run({id.world_id,id.execution_epoch,"run:test"});check(polled.ok&&polled.value.committed_half_days==0,"poll finite parked progress");
    ResumeRun grant{id,"run:test",1,1};auto advanced=world.resume_run(grant);check(advanced.ok&&advanced.value.committed_half_days==1,"one actual halfday permit");check(world.identity().value.tick==1,"actual clock advanced once");auto duplicate=world.resume_run(grant);check(duplicate.ok&&duplicate.value.committed_half_days==1&&world.identity().value.tick==1,"duplicate old-source permit does not repeat");
    auto wrong=RunRef{id.world_id,id.execution_epoch+1,"run:test"};auto refused=world.cancel_run(wrong);check(!refused.ok&&refused.error.original.code=="StaleWorld","wrong epoch cancellation rejected");auto cancelled=world.cancel_run({id.world_id,id.execution_epoch,"run:test"});check(cancelled.ok&&cancelled.value.stop==RunStop::cancelled,"same world cancellation at boundary");
    auto current=world.identity().value;auto reused=world.begin_run({current,"run:test","actor.player",action,4,100000});check(!reused.ok&&reused.error.original.code=="RunIdAlreadyUsed","same-epoch run ID cannot be reused");auto no_more=world.resume_run({current,"run:test",2,3});check(no_more.ok&&no_more.value.stop==RunStop::cancelled&&world.identity().value.tick==1,"cancelled run grants no step");
    auto saved=world.checkpoint();check(saved.ok,"typed checkpoint after actual step");check(world.restore(saved.value,prepared.value).ok,"restore same logical world");auto restored=world.identity().value;check(restored.world_id==current.world_id&&restored.tick==current.tick&&restored.revision==current.revision&&restored.execution_epoch>current.execution_epoch,"restore identity and new execution epoch");check(!world.poll_run({current.world_id,current.execution_epoch,"run:test"}).ok,"old callback cannot poll restored world");check(!world.cancel_run({current.world_id,current.execution_epoch,"run:test"}).ok,"old callback cannot cancel restored world");
    auto round=world.checkpoint();check(round.ok&&round.value==saved.value,"ephemeral epoch excluded from checkpoint");auto truncated=saved.value;truncated.pop_back();check(!world.restore(truncated,prepared.value).ok,"truncated checkpoint rejected");auto after=world.identity().value;check(after.world_id==restored.world_id&&after.execution_epoch==restored.execution_epoch&&after.revision==restored.revision,"failed restore leaves identity unchanged");auto after_bytes=world.checkpoint();check(after_bytes.ok&&after_bytes.value==saved.value,"failed restore leaves complete state bytes unchanged");
    std::cout<<"RESIDENT_KERNEL_LIFECYCLE "<<checks<<" checks "<<failures<<" failures\n";return failures?1:0;
}

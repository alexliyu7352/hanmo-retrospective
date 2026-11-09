// S0维护窄门：只读原代表bank，验证CLI报价/回执与live序号口的本人边界；不造缓存或重玩世界。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/storage/common_codec.hpp"
#include <picosha2.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace hanmo::resident;
namespace {
using V=BoundaryValue;
void require(bool value,const StableId& message){if(!value)throw std::runtime_error(message);}
template<class T>T must(Outcome<T> value,const StableId& message){require(value.ok,message+": "+value.error.original.code);return std::move(value.value);}
void must(Status value,const StableId& message){require(value.ok,message+": "+value.error.original.code);}
StableId read(const StableId& path){std::ifstream file(path,std::ios::binary);require(bool(file),"open "+path);std::ostringstream data;data<<file.rdbuf();return data.str();}
StableId json(const V& value){return must(render_boundary_json(value),"encode");}
bool ok(const V& value){const auto* field=value.find("ok");return field&&field->as<bool>()&&*field->as<bool>();}
void denied(const V& value){require(!ok(value)&&json(value).find("PlayerCommandAuthorityRequired")!=StableId::npos,"expected player authority refusal: "+json(value));}
}

int main(int argc,char** argv){try{
    require(argc==3,"usage: shared_world_foundation_authority_probe PREPARED ORIGINAL_BANK");
    const auto catalog=must(parse_boundary_json(read(argv[1])),"catalog");
    const auto raw=read(argv[2]);const Bytes original(raw.begin(),raw.end());
    auto session=create_resident_replay_session();require(bool(session),"production factory");
    must(session->initialize_checkpoint(catalog,original),"read-only cold restore");
    require(original==must(session->checkpoint(),"initial export"),"cold restore exact original bytes");
    const auto state=must(session->export_state(),"original state");
    using namespace boundary;
    const auto player=get_string(state,"player_actor_id","state");
    const auto& director=field(state,"director","state");
    require(get_int(director,"registry_version","director")==1,"new registry profile required");
    const auto& sequences=field(state,"request_sequences","state");
    const auto& cached=field(state,"results","state");
    StableId other="society::actor.study_caravanner";
    require(other!=player&&field(state,"actors","state").find(other),"real NPC required");
    const auto npc_sequence=integer(field(sequences,other,"sequences"),other);
    Integer npc_cached_records=0;StableId cached_npc;Integer cached_npc_sequence=0;
    for(const auto& row:object(sequences,"sequences"))if(row.first!=player){
        const auto sequence=integer(row.second,row.first);
        for(Integer i=1;i<=sequence;++i)if(cached.find(row.first+":"+std::to_string(i))){++npc_cached_records;if(cached_npc.empty()){cached_npc=row.first;cached_npc_sequence=i;}}
    }
    V::Array checks;
    // 每次受测调用都先后导出完整checkpoint；对比字节不只比较revision/tick。
    const auto unchanged=[&](const Bytes& before,const StableId& label,const V& response){
        const auto after=must(session->checkpoint(),label+" checkpoint");
        require(before==after,label+" mutated checkpoint");
        checks.push_back(obj({{"case",V{label}},{"checkpoint_exact",V{true}},{"before_sha256",V{picosha2::hash256_hex_string(before)}},{"after_sha256",V{picosha2::hash256_hex_string(after)}},{"response",response}}));
    };
    const auto request=[&](const StableId& actor,Integer sequence,Integer revision){return obj({{"kind",V{"move_local"}},{"params",obj({{"place_id",V{"society::place.grain"}}})},{"actor_id",V{actor}},{"request_seq",V{sequence}},{"expected_revision",V{revision}}});};
    {
        const auto before=must(session->checkpoint(),"before NPC CLI");
        const auto result=must(session->execute(request(other,npc_sequence+1,must(session->identity(),"source").revision)),"actual CLI execute");
        denied(result.preview);denied(result.receipt);
        unchanged(before,"other_actor_cli_preview_and_receipt",obj({{"preview",result.preview},{"receipt",result.receipt}}));
    }
    if(!cached_npc.empty()){
        const auto before=must(session->checkpoint(),"before cached NPC CLI");
        const auto result=must(session->execute(request(cached_npc,cached_npc_sequence,0)),"actual cached NPC execute");
        denied(result.preview);denied(result.receipt);
        unchanged(before,"existing_other_actor_cached_receipt_denied",obj({{"actor_id",V{cached_npc}},{"request_seq",V{cached_npc_sequence}},{"preview",result.preview},{"receipt",result.receipt}}));
    }
    const auto context=[&](const StableId& actor){LiveOperation operation;operation.kind=LiveOperationKind::query;operation.source=must(session->identity(),"context source");operation.payload=obj({{"query_id",V{"command_context"}},{"args",obj({{"actor_id",V{actor}}})}});return must(session->execute_live(operation),"actual live command_context").result;};
    {
        const auto before=must(session->checkpoint(),"before other context");const auto result=context(other);denied(result);
        unchanged(before,"other_actor_live_command_context",result);
    }
    {
        const auto before=must(session->checkpoint(),"before own context");const auto result=context(player);
        require(ok(result)&&get_string(result,"actor_id","context")==player&&get_int(result,"next_request_seq","context")==integer(field(sequences,player,"sequences"),player)+1,"own lawful context");
        unchanged(before,"player_live_command_context",result);
    }
    {
        // 原producer的第一条真实命令是本人move_local到grain；以原序号/旧revision重放既有receipt。
        const auto* expected=cached.find(player+":1");require(expected&&ok(*expected)&&get_string(*expected,"kind","cached")=="move_local","original player move receipt");
        const auto before=must(session->checkpoint(),"before own cached replay");
        const auto result=must(session->execute(request(player,1,0)),"actual own cached execute");
        require(compare_boundary(*expected,result.receipt).exact(),"original cached receipt exact");
        unchanged(before,"player_existing_cached_receipt",result.receipt);
    }
    require(original==must(session->checkpoint(),"final checkpoint"),"all operations preserve original bytes");
    std::cout<<json(obj({{"ok",V{true}},{"bank_sha256",V{picosha2::hash256_hex_string(original)}},{"tick",V{must(session->identity(),"final identity").tick}},{"npc_request_sequence",V{npc_sequence}},{"npc_cached_receipt_count",V{npc_cached_records}},{"existing_npc_cached_replay_exercised",V{!cached_npc.empty()}},{"coverage_note",V{cached_npc.empty()?"Original bank contains no NPC external cached receipt; that replay branch is not exercised and no record was fabricated.":"Actual original NPC cached receipt was denied before replay."}},{"checks",V{std::move(checks)}}}))<<'\n';
    return 0;
}catch(const std::exception& error){std::cerr<<"shared_world_foundation_authority_probe: FAIL "<<error.what()<<'\n';return 1;}}

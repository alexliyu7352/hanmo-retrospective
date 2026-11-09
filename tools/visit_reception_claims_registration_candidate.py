"""固定ed1追加独立来访认领尾；中央串行合流，旧Director/Interaction visitor字节不变。"""
import argparse, hashlib, json, subprocess
from pathlib import Path
from visit_message_registration_candidate import replace, after
BASE='ed1c5e0c56f1c3bc423dc718c8d7596c47e825a1'
ROOT='native/world/src/resident/domains/interaction/visit_messages/'
SHARED=['native/world/src/resident/domains/interaction/interaction_model.hpp','native/world/src/resident/kernel/native_initial_state.cpp','native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/storage/common_codec.cpp','game/ui/native_resident_backend.gd']
OWNED=[ROOT+n for n in ['visit_reception_state.hpp','visit_reception_claims.hpp','visit_reception_claims.cpp','visit_reception_codec.cpp','visit_reception_catalog.hpp','visit_reception_catalog.cpp','visit_reception_director.hpp','visit_reception_director.cpp','visit_reception_world.hpp','visit_reception_world.cpp']]+['tools/visit_reception_claims_registration_candidate.py','native/world/tests/resident/visit_reception_test.cpp','native/world/tests/resident/visit_reception_claims_test.cpp','game/tests/test_visit_reception_catalog.gd']

def transform(path,s):
    """每个既有锚点恰命中一次；不会把旧目录或旧shared整叶写回中央。"""
    if path.endswith('/interaction_model.hpp'):
        s='#include "visit_messages/visit_reception_state.hpp"\n'+s
        return replace(s,'    std::optional<VisitMessageState> visit_messages;','    std::optional<VisitMessageState> visit_messages;\n    std::optional<VisitReceptionState> visit_reception_claims;')
    if path.endswith('/native_initial_state.cpp'):
        s=after(s,'if(visit_reception.value)admitted.module_versions.emplace_back("visit_reception_intents","1");','    if(visit_reception.value)admitted.module_versions.emplace_back("visit_reception_claims","1");')
        return replace(s,'if(mutable_catalog->interaction->visit_messages)interaction->visit_messages.emplace();','if(mutable_catalog->interaction->visit_messages)interaction->visit_messages.emplace();if(mutable_catalog->director&&mutable_catalog->director->visit_reception)interaction->visit_reception_claims.emplace();')
    if path.endswith('/native_resident_backend.gd'):
        return after(s,'expected["module_versions"].append({"name":"visit_reception_intents"','\tif definition.get("visit_reception_profile","")=="authored_local_reception_v1": expected["module_versions"].append({"name":"visit_reception_claims","version":"1"})')
    if path.endswith('/common_codec.cpp'):
        s=replace(s,'std::string domain=(kind=="visit_message_posted"','std::string domain=kind=="visit_reception_claimed"?"visit_reception_claims":(kind=="visit_message_posted"')
        s=replace(s,'auto named=string(*declared,row.first);','auto named=string(*declared,row.first);if(kind=="visit_reception_claimed"&&named!="visit_reception_claims")fail("InvalidFactDomain",row.first);')
        return replace(s,'if(!(named=="visit_messages"','if(!(named=="visit_reception_claims"&&kind=="visit_reception_claimed")&&!(named=="visit_messages"')
    if path.endswith('/world_codec.cpp'):
        s=replace(s,'if(catalog.interaction&&catalog.interaction->visit_messages)domains.push_back({"visit_messages",1});return domains;','if(catalog.interaction&&catalog.interaction->visit_messages)domains.push_back({"visit_messages",1});if(catalog.director&&catalog.director->visit_reception)domains.push_back({"visit_reception_claims",1});return domains;')
        s=replace(s,'{"visit_reception_intents",{"1"}},','{"visit_reception_intents",{"1"}}, {"visit_reception_claims",{"1"}},')
        s=replace(s,'if(has("visit_reception_intents")&&(!dynamic||!has("visit_messages")))fail("VisitReceptionProfileMismatch","checkpoint/module_versions");','if(has("visit_reception_intents")!=has("visit_reception_claims")||(has("visit_reception_intents")&&(!dynamic||!has("visit_messages"))))fail("VisitReceptionProfileMismatch","checkpoint/module_versions");if(has("visit_reception_claims"))domains.push_back({"visit_reception_claims",1});')
        s=replace(s,'if(has("visit_reception_intents","1")&&(!dynamic||!has("visit_messages","1")))return {false,{},common_failure("VisitReceptionProfileMismatch").error};','if(has("visit_reception_intents","1")!=has("visit_reception_claims","1")||has("visit_reception_claims","1")!=(version("visit_reception_claims")==1)||(has("visit_reception_intents","1")&&(!dynamic||!has("visit_messages","1"))))return {false,{},common_failure("VisitReceptionProfileMismatch").error};')
        s=after(s,'if(result->director&&result->director->visit_reception)id.module_versions.emplace_back("visit_reception_intents","1");','    if(result->director&&result->director->visit_reception)id.module_versions.emplace_back("visit_reception_claims","1");')
        s=replace(s,'result.interaction=std::make_shared<const InteractionState>(std::move(i.value));','if(catalog.director&&catalog.director->visit_reception){auto decoded=decode_visit_reception_claims(field(source,"visit_reception_claims","state"));if(!decoded.ok)return {false,{},decoded.error};i.value.visit_reception_claims=std::move(decoded.value);}else if(source.find("visit_reception_claims"))fail("UnexpectedVisitReceptionClaims","visit_reception_claims");result.interaction=std::make_shared<const InteractionState>(std::move(i.value));')
        s=replace(s,'||(w.catalog->interaction->visit_messages&&w.interaction.visit_messages&&header->domain=="visit_messages"','||(w.catalog->director&&w.catalog->director->visit_reception&&w.interaction.visit_reception_claims&&header->domain==visit_reception_claim_module&&header->kind==visit_reception_claim_kind&&w.interaction.visit_reception_claims->claims.find(row.first))||(w.catalog->interaction->visit_messages&&w.interaction.visit_messages&&header->domain=="visit_messages"')
        s=after(s,'set(output,"visit_messages",encode_visit_messages','    if(w.catalog->director&&w.catalog->director->visit_reception){if(!w.interaction.visit_reception_claims)fail("MissingVisitReceptionClaims","visit_reception_claims");set(output,"visit_reception_claims",encode_visit_reception_claims(*w.interaction.visit_reception_claims));}')
        s=after(s,'r=section("visit_messages",','    if(w.catalog->director&&w.catalog->director->visit_reception){if(!w.interaction.visit_reception_claims)return {false,{},common_failure("MissingVisitReceptionClaims").error};r=section("visit_reception_claims",[&](auto& c){return visit_visit_reception_checkpoint(c,*w.interaction.visit_reception_claims);});if(!r.ok)return {false,{},r.error};}')
        return after(s,'else if(section.domain=="visit_messages")','        else if(section.domain=="visit_reception_claims"){i->visit_reception_claims.emplace();r=visit_visit_reception_checkpoint(reader,*i->visit_reception_claims);}')
    raise ValueError(path)

def main():
    """独立新输出保存精确基准与owned快照，不覆写已有候选或中央。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--owned-repo',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--wip',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest();head=subprocess.check_output(['git','-C',str(a.owned_repo),'rev-parse','HEAD'],text=True).strip();shared=[];owned=[]
    for path in SHARED:
        old=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);data=transform(path,old.decode()).encode();out=a.output/'candidate'/path;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(data);shared.append({'path':path,'base_oid':oid(old),'result_oid':oid(data)})
    for path in OWNED:
        data=(a.owned_repo/path).read_bytes()
        if not a.wip:assert data==subprocess.check_output(['git','-C',str(a.owned_repo),'show',head+':'+path]),path
        out=a.output/'candidate'/path;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(data);owned.append({'path':path,'oid':oid(data)})
    (a.output/'manifest.json').write_text(json.dumps({'base':a.base,'source':head,'wip':a.wip,'shared':shared,'owned':owned,'state':'SOURCE_ONLY','new_cpp':[ROOT+'visit_reception_claims.cpp',ROOT+'visit_reception_codec.cpp'],'new_headers':[ROOT+'visit_reception_state.hpp',ROOT+'visit_reception_claims.hpp'],'contract':'visit_reception_intents/1 + visit_reception_claims/1 exact pair; original visitors unchanged; same Talk transaction'},indent=2)+'\n');print(a.output/'manifest.json')
if __name__=='__main__':main()

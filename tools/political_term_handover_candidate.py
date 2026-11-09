#!/usr/bin/env python3
"""从固定已含首轮与生前交接的Git树生成后任生前交接隔离候选；不修改中央共享叶。"""
import argparse,difflib,hashlib,json,subprocess,sys
from pathlib import Path

def main():
    """逐严格锚点增加独立尾和三命令，原模块、访客与任职来源保持原字节。"""
    p=argparse.ArgumentParser()
    for key in ('repo','base','output'):p.add_argument('--'+key,required=True)
    p.add_argument('--base-candidate');a=p.parse_args();root=Path(a.repo);out=Path(a.output);files={};before={}
    overlay=Path(a.base_candidate) if a.base_candidate else None
    prior=json.loads((overlay/'manifest.json').read_text()) if overlay else None
    prior_rows={v['path']:v for v in prior['files']} if prior else {}
    if (out/'manifest.json').exists():raise RuntimeError('Immutable candidate already exists')
    def read(path):
        """共享输入只来自指定Git对象，不从另一开发位借移动字节。"""
        if path not in files:
            if path in prior_rows:
                raw=(overlay/'candidate'/path).read_bytes(); expected=prior_rows[path]
                if hashlib.sha256(raw).hexdigest()!=expected['sha256']:raise RuntimeError('Modified frozen base candidate: '+path)
                before[path]=raw.decode()
            else:before[path]=subprocess.check_output(['git','show',a.base+':'+path],cwd=root).decode()
            files[path]=before[path]
        return files[path]
    def rep(path,old,new,count=1):
        """锚点计数不一致即停，避免静默漏接或改动同名其他来源。"""
        s=read(path)
        if s.count(old)!=count:raise RuntimeError(f'{path}: {old[:90]!r}: {s.count(old)} != {count}')
        files[path]=s.replace(old,new)
    def after(path,old,new):
        """仅在原完整表达式之后增加同类独立接点。"""
        rep(path,old,old+new)
    def tr(s):
        """转换已核同形接口名；业务源数与类型另行明确修正。"""
        for x,y in [('PoliticalHeadHandover','PoliticalTermHandover'),('political_head_handovers','political_term_handovers'),('political_head_handover','political_term_handover'),('handover_version','term_version'),('handover_modules','term_modules'),('handover_ids','term_ids'),('handovers','term_handovers'),('handover','term_handover')]:
            # 避免新名字再被通用变量替换一遍。
            if x in ('handovers','handover'):continue
            s=s.replace(x,y)
        return s.replace('const auto* handover=','const auto* term_handover=').replace('handover!=nullptr','term_handover!=nullptr').replace('(handover&&','(term_handover&&').replace('(*handover)','(*term_handover)').replace('if(handover)','if(term_handover)').replace('id,*handover','id,*term_handover').replace('auto handovers=','auto term_handovers=').replace('!handovers.','!term_handovers.').replace('handovers.error','term_handovers.error').replace('if(handovers.value)','if(term_handovers.value)').replace('*handovers.value','*term_handovers.value')
    def round_key(s):
        """同类新条款接在已冻结新轮之后，三处module及二进制段保持同序。"""
        return s.replace('PoliticalHeadHandover','PoliticalSuccessorRound').replace('political_head_handovers','political_successor_rounds').replace('political_head_handover','political_successor_round').replace('handover_version','round_version').replace('handover_modules','round_modules').replace('handover_ids','round_ids').replace('auto handovers=','auto rounds=').replace('const auto* handover=','const auto* next_round=').replace('if(handover)','if(next_round)').replace('political_successor_round_domain','political_successor_rounds_domain')
    def line(path,key):
        """具名原接点必须唯一，防止重复拷贝同一大函数。"""
        xs=[x for x in read(path).splitlines(True) if key in x]
        if len(xs)!=1:raise RuntimeError(f'{path}: ambiguous {key}: {len(xs)}')
        return xs[0]
    def clone(path,key,edit=lambda s:s):
        """在既有同类段后追加独立新段，保原排列与旧选择逻辑。"""
        old=line(path,key);anchor=line(path,round_key(key));after(path,anchor,edit(tr(old)))
    K='native/world/src/resident/kernel/';W=K+'world_bundle.hpp';B=K+'world_bundle.cpp';R=K+'resident_world.cpp';C=K+'world_codec.cpp'
    M='native/world/include/hanmo/resident/model_contract.hpp';S='native/world/src/resident/model/common_model.hpp'
    after(M,'struct PoliticalSuccessorRoundState;struct PoliticalSuccessorRoundCatalog;','struct PoliticalTermHandoverState;struct PoliticalTermHandoverCatalog;')
    clone(M,'std::shared_ptr<const PoliticalHeadHandoverCatalog>')
    clone(S,'std::shared_ptr<const PoliticalHeadHandoverState>')
    clone(W,'#include "political_head_handover_world.hpp"')
    clone(W,'std::optional<PoliticalHeadHandoverState>')
    rep(W,'const StableId& political_successor_round_source_id={});','const StableId& political_successor_round_source_id={},const StableId& political_term_handover_source_id={});')
    for old in ['political_successor_rounds(seed.political_successor_rounds?std::optional<PoliticalSuccessorRoundState>{*seed.political_successor_rounds}:std::nullopt),','political_successor_rounds(other.political_successor_rounds),']:after(B,old,old.replace('PoliticalSuccessorRound','PoliticalTermHandover').replace('political_successor_rounds','political_term_handovers'))
    clone(B,'valid=validate_political_head_handover_world(*this);')
    clone(B,'if(w.political_head_handovers){std::vector<StableId> ids;')
    D=K+'command_types.hpp';clone(D,'#include "political_head_handover_world.hpp"')
    for prefix,item in [('using CommandData=','PoliticalTermHandoverCommand'),('using QuoteData=','PoliticalTermHandoverQuote')]:
        old=line(D,prefix);rep(D,old,old.replace('>;',','+item+'>;'))
    clone(K+'phase_plan.cpp','applied=false;if(std::holds_alternative<PoliticalHeadHandoverCommand>')
    clone(K+'dynamic_integration.cpp','if(proposal.command&&(std::holds_alternative<PoliticalHeadHandoverCommand>')
    start='StableId political_head_handover_source_id;';middle='StableId political_successor_round_source_id;';end='StableId invitation_reply_source_id;';text=read(R);old=text[text.index(start):text.index(middle)];rep(R,end,tr(old)+end)
    rep(R,'political_successor_round_source_id);','political_successor_round_source_id,political_term_handover_source_id);')
    rep(R,'const StableId& political_successor_round_source_id){','const StableId& political_successor_round_source_id,const StableId& political_term_handover_source_id){')
    clone(R,'auto bound=bind_political_head_handover_result_world(')
    D=K+'dispatcher.cpp'
    for key in ['if(is_political_head_handover_command(kind)&&!w.catalog','const auto* handover=std::get_if<PoliticalHeadHandoverCommand>','if(handover){auto q=','if(const auto* c=std::get_if<PoliticalHeadHandoverCommand>(&op.command.data))','if(const auto* c=std::get_if<PoliticalHeadHandoverCommand>(&command.data))']:clone(D,key)
    N=K+'native_initial_state.cpp';clone(N,'if(mutable_catalog->political_head_handovers)seed.');clone(N,'auto handovers=decode_political_head_handover_profile(')
    clone(K+'live_view_queries.cpp','if(query=="political_head_handovers")')
    V='native/world/src/resident/checkpoint/codec_visitor.hpp';clone(V,'Integer political_head_handover_codec_version=0;')
    D='native/world/src/resident/checkpoint/command_boundary.cpp'
    for key in ['if(is_political_head_handover_command(kind)){auto effect=','if(is_political_head_handover_command(r.kind)&&','if(is_political_head_handover_command(value.kind)&&c.political_head_handover_codec_version','if(is_political_head_handover_command(kind)){auto decoded=','else if constexpr(std::is_same_v<T,PoliticalHeadHandoverQuote>)']:clone(D,key)
    old='if(is_political_head_handover_command(kind)&&(c.political_head_handover_codec_version!=1||tag!=12))return c.reject("InvalidPoliticalHeadHandoverReceipt");';after(D,old,tr(old))
    old='||(is_political_head_handover_command(kind)&&c.political_head_handover_codec_version==1)';after(D,old,tr(old))
    old='if(is_political_head_handover_command(kind)&&!valid_political_head_handover_effect(kind,value))return c.reject("InvalidPoliticalHeadHandoverReceipt");';after(D,old,tr(old))
    old='if(is_political_head_handover_command(value.kind)&&!has_details)return c.reject("MissingPoliticalHeadHandoverReceipt");';after(D,old,tr(old))
    D='native/world/src/resident/storage/common_codec.cpp'
    old='const bool political_head_handover=kind=="political_head_handover_offered"||kind=="political_head_handover_accepted"||kind=="political_head_handover_declined"||kind=="political_head_handover_withdrawn";'
    after(D,old,'const bool political_term_handover=kind=="political_term_handover_offered"||kind=="political_term_handover_accepted"||kind=="political_term_handover_declined"||kind=="political_term_handover_withdrawn";if(political_term_handover)fields(row.second,{"kind","domain"},{},row.first);')
    rep(D,'std::string domain=political_successor_round?','std::string domain=political_term_handover?"political_term_handovers":political_successor_round?')
    old='if(political_head_handover!=(named=="political_head_handovers"))fail("InvalidFactDomain",row.first);';after(D,old,tr(old))
    rep(D,'if(!(named=="political_successor_rounds"&&political_successor_round)','if(!(named=="political_term_handovers"&&political_term_handover)&&!(named=="political_successor_rounds"&&political_successor_round)')
    old='if(catalog.political_successor_rounds)domains.push_back({political_successor_rounds_domain,1});';after(C,old,'if(catalog.political_term_handovers)domains.push_back({political_term_handover_domain,1});')
    after(C,'{"political_successor_rounds",{"1"}},',' {"political_term_handovers",{"1"}},')
    for key in ['auto handover_version=political_head_handover_selected_version(identity.catalog.module_versions)','auto handover_version=political_head_handover_selected_version(versions)','auto handovers=decode_political_head_handover_profile(scenario,caps);','if(result->political_head_handovers)id.module_versions','if(catalog.political_head_handovers){auto parsed=decode_political_head_handover_state','if(legal_header&&(is_political_head_handover_fact_kind','if(w.catalog->political_head_handovers){if(!w.political_head_handovers)fail(','auto handover_version=political_head_handover_selected_version(w.catalog->identity.module_versions)','if(w.catalog->political_head_handovers){if(!w.political_head_handovers)return','auto handover_version=political_head_handover_selected_version(decoded.value.identity.catalog.module_versions)','auto handover_version=political_head_handover_selected_version(catalog.identity.module_versions)','else if(section.domain==political_head_handover_domain)']:clone(C,key)
    clone(C,'std::set<StableId> handover_ids;')
    for old,count in [('writer.political_head_handover_codec_version=handover_version.value;',1),('reader.political_head_handover_codec_version=handover_version.value;',2)]:rep(C,old,old+tr(old),count)
    after(K+'political_successor_round_world.cpp','#include "political_successor_round_world.hpp"','\n#include "political_term_handover_world.hpp"')
    rep(K+'political_successor_round_world.cpp','w.political_head_handovers?&*w.political_head_handovers:nullptr},','w.political_head_handovers?&*w.political_head_handovers:nullptr,political_term_handover_head_read_world(w)},')
    UI='game/ui/native_resident_backend.gd';clone(UI,'if definition.get("dynamic_world",{}).has("political_head_handovers"):')
    old='func political_head_handovers_enabled()';text=read(UI);start=text.index(old);end=text.index('\n',text.index('return demand_view("political_head_handovers"',start))+1;block=text[start:end];anchor=read(UI);rs=anchor.index('func political_successor_rounds_enabled()');re=anchor.index('\n',anchor.index('return demand_view("political_successor_rounds"',rs))+1;after(UI,anchor[rs:re],'\n## 已正式就任者的本人让职页。\n'+tr(block))
    for path in ['game/ui/resident_page_requests.gd','game/ui/screen_composer.gd']:clone(path,'const PoliticalHeadHandoverPanel=preload(')
    clone('game/ui/resident_page_requests.gd','if not PoliticalHeadHandoverPanel.prepare(host,')
    path='game/ui/screen_composer.gd';old=line(path,'if not current_place.is_empty() and kernel.has_method("political_head_handovers_enabled")')+line(path,'PoliticalHeadHandoverPanel.render(');anchor=line(path,round_key('if not current_place.is_empty() and kernel.has_method("political_head_handovers_enabled")'))+line(path,round_key('PoliticalHeadHandoverPanel.render('));after(path,anchor,tr(old).replace('原首职生前交接','后任首职生前交接'))
    L='tools/content/links.py';clone(L,'from .political_head_handover_links import');rep(L,'check_political_head_handovers(pack,index)','check_political_head_handovers(pack,index) + check_political_term_handovers(pack,index)')
    L='tools/content/dynamic_world_links.py';old="allowed.update({'political_offer_head_handover', 'political_withdraw_head_handover'})";text=read(L);s=text.rfind('            successions =',0,text.index(old));e=text.index('\n',text.index(old));block=text[s:e];after(L,block,'\n'+block.replace('political_head_handovers','political_term_handovers').replace('political_offer_head_handover','political_offer_term_handover').replace('political_withdraw_head_handover','political_withdraw_term_handover'))
    rep('tools/verify.py','TESTS = [',"TESTS = ['test_political_term_handover_panel', ")
    sys.path.insert(0,str(root/'tools'));from content.political_term_handover_links import with_political_term_handover_schema
    path='schemas/content.schema.json';files[path]=json.dumps(with_political_term_handover_schema(json.loads(read(path))),ensure_ascii=False,indent=2)+'\n'
    path='game/data/rule_capabilities.json';value=json.loads(read(path));value['capabilities'].append({'capability_id':'political_term_handovers_v1','rule_version':'political_term_handovers_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.political_term_handovers.version','equals':1}],'supported_semantics':[],'test_ids':['test_political_term_handover_panel'],'evidence':[]});files[path]=json.dumps(value,ensure_ascii=False,indent=2)+'\n'
    for path in [K+'political_term_handover_world.hpp',K+'political_term_handover_world.cpp']:before[path]='';files[path]=(root/path).read_text()
    domain=root/'native/world/src/resident/domains/politics/term_handover'
    manifest={'base':a.base,'classification':'SHARED_CANDIDATE_NOT_CENTRAL_OWNED','status':'SOURCE_ONLY_UNREGISTERED','cmake':'central sole writer','base_candidate':str(overlay) if overlay else None,'base_candidate_sha256':hashlib.sha256((overlay/'manifest.json').read_bytes()).hexdigest() if overlay else None,'dependencies':[],'files':[],'cmake_registration':{'product_cpp':[str(x.relative_to(root)) for x in sorted(domain.glob('*.cpp'))]+[K+'political_term_handover_world.cpp'],'headers':[str(x.relative_to(root)) for x in sorted(domain.glob('*.hpp'))]+[K+'political_term_handover_world.hpp'],'existing_product_delta':['native/world/src/resident/domains/politics/political_member_history.cpp','native/world/src/resident/domains/politics/political_member_history.hpp','native/world/src/resident/domains/politics/successor_rounds/political_head_source.cpp','native/world/src/resident/domains/politics/successor_rounds/political_head_source.hpp','native/world/src/resident/domains/politics/successor_rounds/political_successor_round_head.cpp'],'native_tests':[str(x.relative_to(root)) for x in sorted((root/'native/world/tests/resident').glob('political_term_handover*_test.cpp'))],'python_tests':['tools/test_political_term_handover_authoring.py','tools/test_political_term_handover_registration.py'],'gd_test':'game/tests/test_political_term_handover_panel.gd'}}
    def oid(text):
        """按实际UTF8字节计算Git blob，不以路径或时间代身份。"""
        b=text.encode();return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    patch=[]
    for path,text in sorted(files.items()):
        target=out/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(text);manifest['files'].append({'path':path,'before_oid':oid(before[path]) if before[path] else None,'after_oid':oid(text),'sha256':hashlib.sha256(text.encode()).hexdigest()});patch.extend(difflib.unified_diff(before[path].splitlines(True),text.splitlines(True),fromfile='a/'+path if before[path] else '/dev/null',tofile='b/'+path))
    (out/'candidate.patch').write_text(''.join(patch));(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps({'files':len(files),'manifest':str(out/'manifest.json')}))

if __name__=='__main__':main()

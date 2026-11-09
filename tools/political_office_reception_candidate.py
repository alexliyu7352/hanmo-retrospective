#!/usr/bin/env python3
"""从固定完整政治Git对象生成公共接待隔离共享候选，绝不回写中央或移动工区。"""
import argparse,difflib,hashlib,json,subprocess,sys
from pathlib import Path

def main():
    """逐唯一锚接独立module、角色候选和原实际结果尾，旧访客与旧指针参数保留。"""
    p=argparse.ArgumentParser()
    for name in ('repo','base','output'):p.add_argument('--'+name,required=True)
    a=p.parse_args();root=Path(a.repo);out=Path(a.output);files={};before={}
    if (out/'manifest.json').exists():raise RuntimeError('Immutable candidate already exists')
    def read(path):
        """共享字节只读指定Git对象，禁止借中央滚动稿。"""
        if path not in files:files[path]=before[path]=subprocess.check_output(['git','show',a.base+':'+path],cwd=root).decode()
        return files[path]
    def rep(path,old,new,count=1):
        """精确锚和计数不符立即停止，不用相似文本猜接点。"""
        s=read(path)
        if s.count(old)!=count:raise RuntimeError(f'{path}: {old[:100]!r}: {s.count(old)} != {count}')
        files[path]=s.replace(old,new)
    def after(path,old,new):rep(path,old,old+new)
    def tr(s):
        """只改已核同形类型/函数名；单来源与旧三表的差异另行明确。"""
        for old,new in [('PoliticalTermHandover','PoliticalOfficeReception'),('political_term_handovers','political_office_receptions'),('political_term_handover','political_office_reception'),('term_handover','office_reception'),('term_handovers','office_receptions'),('term_version','reception_version'),('term_modules','reception_modules'),('term_ids','reception_ids')]:s=s.replace(old,new)
        return s.replace('->offers.size()+result.political_office_receptions->answers.size()+result.political_office_receptions->withdrawals.size()','->assignments.size()')
    def line(path,key):
        """整行复用只用于独立表达式，复合大函数的内部接点另用exact片段。"""
        rows=[x for x in read(path).splitlines(True) if key in x]
        if len(rows)!=1:raise RuntimeError(f'{path}: ambiguous {key}: {len(rows)}')
        return rows[0]
    def clone(path,key,edit=lambda x:x):
        old=line(path,key);after(path,old,edit(tr(old)))
    K='native/world/src/resident/kernel/';W=K+'world_bundle.hpp';B=K+'world_bundle.cpp';R=K+'resident_world.cpp';C=K+'world_codec.cpp';M='native/world/include/hanmo/resident/model_contract.hpp';S='native/world/src/resident/model/common_model.hpp'
    after(M,'struct PoliticalTermHandoverState;struct PoliticalTermHandoverCatalog;','struct PoliticalOfficeReceptionState;struct PoliticalOfficeReceptionCatalog;')
    clone(M,'std::shared_ptr<const PoliticalTermHandoverCatalog>');clone(S,'std::shared_ptr<const PoliticalTermHandoverState>');clone(W,'#include "political_term_handover_world.hpp"');clone(W,'std::optional<PoliticalTermHandoverState>')
    rep(W,'const StableId& political_term_handover_source_id={});','const StableId& political_term_handover_source_id={},const StableId& political_office_reception_source_id={});')
    for old in ['political_term_handovers(seed.political_term_handovers?std::optional<PoliticalTermHandoverState>{*seed.political_term_handovers}:std::nullopt),','political_term_handovers(other.political_term_handovers),']:after(B,old,tr(old))
    clone(B,'valid=validate_political_term_handover_world(*this);')
    old=line(B,'if(w.political_term_handovers){std::vector<StableId> ids;');new=tr(old);start=new.index('for(');end=new.index('for(',start+4) if 'for(' in new[start+4:] else -1
    # 原事实登记只新增一个表；不能继承旧让职三表枚举。
    new=new.replace('w.political_office_receptions->offers','w.political_office_receptions->assignments')
    import re
    new=re.sub(r'for\(const auto& row:w\.political_office_receptions->(?:answers|withdrawals)\.rows\(\)\)ids\.push_back\(row.first\);','',new)
    if '->answers' in new or '->withdrawals' in new:raise RuntimeError('Unexpected original payload registration shape')
    after(B,old,new)
    D=K+'command_types.hpp';clone(D,'#include "political_term_handover_world.hpp"')
    for prefix,item in [('using CommandData=','PoliticalOfficeReceptionCommand'),('using QuoteData=','PoliticalOfficeReceptionQuote')]:
        old=line(D,prefix);rep(D,old,old.replace('>;',','+item+'>;'))
    clone(K+'phase_plan.cpp','applied=false;if(std::holds_alternative<PoliticalTermHandoverCommand>')
    clone(K+'dynamic_integration.cpp','if(proposal.command&&(std::holds_alternative<PoliticalTermHandoverCommand>')
    start='StableId political_term_handover_source_id;';end='StableId invitation_reply_source_id;';text=read(R);old=text[text.index(start):text.index(end)];rep(R,end,tr(old)+end)
    rep(R,'political_term_handover_source_id);','political_term_handover_source_id,political_office_reception_source_id);')
    rep(R,'const StableId& political_term_handover_source_id){','const StableId& political_term_handover_source_id,const StableId& political_office_reception_source_id){')
    clone(R,'auto bound=bind_political_term_handover_result_world(')
    D=K+'dispatcher.cpp'
    for key in ['if(is_political_term_handover_command(kind)&&!w.catalog','const auto* term_handover=std::get_if<PoliticalTermHandoverCommand>','if(term_handover){auto q=','if(const auto* c=std::get_if<PoliticalTermHandoverCommand>(&op.command.data))','if(const auto* c=std::get_if<PoliticalTermHandoverCommand>(&command.data))']:clone(D,key)
    N=K+'native_initial_state.cpp';clone(N,'if(mutable_catalog->political_term_handovers)seed.');clone(N,'auto term_handovers=decode_political_term_handover_profile(')
    clone(K+'live_view_queries.cpp','if(query=="political_term_handovers")')
    V='native/world/src/resident/checkpoint/codec_visitor.hpp';clone(V,'Integer political_term_handover_codec_version=0;')
    D='native/world/src/resident/checkpoint/command_boundary.cpp'
    for key in ['if(is_political_term_handover_command(kind)){auto effect=','if(is_political_term_handover_command(r.kind)&&','if(is_political_term_handover_command(value.kind)&&c.political_term_handover_codec_version','if(is_political_term_handover_command(kind)){auto decoded=','else if constexpr(std::is_same_v<T,PoliticalTermHandoverQuote>)']:clone(D,key)
    for old in ['if(is_political_term_handover_command(kind)&&(c.political_term_handover_codec_version!=1||tag!=12))return c.reject("InvalidPoliticalTermHandoverReceipt");','||(is_political_term_handover_command(kind)&&c.political_term_handover_codec_version==1)','if(is_political_term_handover_command(kind)&&!valid_political_term_handover_effect(kind,value))return c.reject("InvalidPoliticalTermHandoverReceipt");','if(is_political_term_handover_command(value.kind)&&!has_details)return c.reject("MissingPoliticalTermHandoverReceipt");']:after(D,old,tr(old))
    D='native/world/src/resident/storage/common_codec.cpp'
    old='const bool political_term_handover=kind=="political_term_handover_offered"||kind=="political_term_handover_accepted"||kind=="political_term_handover_declined"||kind=="political_term_handover_withdrawn";'
    after(D,old,'const bool political_office_reception=kind=="political_office_reception_assumed";if(political_office_reception)fields(row.second,{"kind","domain"},{},row.first);')
    rep(D,'std::string domain=political_term_handover?','std::string domain=political_office_reception?"political_office_receptions":political_term_handover?')
    old='if(political_term_handover!=(named=="political_term_handovers"))fail("InvalidFactDomain",row.first);';after(D,old,tr(old))
    rep(D,'if(!(named=="political_term_handovers"&&political_term_handover)','if(!(named=="political_office_receptions"&&political_office_reception)&&!(named=="political_term_handovers"&&political_term_handover)')
    old='if(catalog.political_term_handovers)domains.push_back({political_term_handover_domain,1});';after(C,old,tr(old));after(C,'{"political_term_handovers",{"1"}},',' {"political_office_receptions",{"1"}},')
    for key in ['auto term_version=political_term_handover_selected_version(identity.catalog.module_versions)','auto term_version=political_term_handover_selected_version(versions)','auto term_handovers=decode_political_term_handover_profile(scenario,caps);','if(result->political_term_handovers)id.module_versions','if(catalog.political_term_handovers){auto parsed=decode_political_term_handover_state','if(legal_header&&(is_political_term_handover_fact_kind','if(w.catalog->political_term_handovers){if(!w.political_term_handovers)fail(','auto term_version=political_term_handover_selected_version(w.catalog->identity.module_versions)','if(w.catalog->political_term_handovers){if(!w.political_term_handovers)return','auto term_version=political_term_handover_selected_version(decoded.value.identity.catalog.module_versions)','auto term_version=political_term_handover_selected_version(catalog.identity.module_versions)','else if(section.domain==political_term_handover_domain)']:clone(C,key)
    clone(C,'std::set<StableId> term_ids;')
    for old,count in [('writer.political_term_handover_codec_version=term_version.value;',1),('reader.political_term_handover_codec_version=term_version.value;',2)]:rep(C,old,old+tr(old),count)
    UI='game/ui/native_resident_backend.gd';clone(UI,'if definition.get("dynamic_world",{}).has("political_term_handovers"):')
    text=read(UI);start=text.index('func political_term_handovers_enabled()');end=text.index('\n',text.index('return demand_view("political_term_handovers"',start))+1;block=text[start:end];after(UI,block,'\n## 公共接待岗位本人认领页。\n'+tr(block))
    for path in ['game/ui/resident_page_requests.gd','game/ui/screen_composer.gd']:clone(path,'const PoliticalTermHandoverPanel=preload(')
    clone('game/ui/resident_page_requests.gd','if not PoliticalTermHandoverPanel.prepare(host,')
    path='game/ui/screen_composer.gd';old=line(path,'if not current_place.is_empty() and kernel.has_method("political_term_handovers_enabled")')+line(path,'PoliticalTermHandoverPanel.render(');after(path,old,tr(old).replace('后任首职生前交接','公共接待岗位'))
    L='tools/content/links.py';clone(L,'from .political_term_handover_links import');rep(L,'check_political_term_handovers(pack,index)','check_political_term_handovers(pack,index) + check_political_office_receptions(pack,index)')
    L='tools/content/dynamic_world_links.py';old="allowed.update({'political_offer_term_handover', 'political_withdraw_term_handover'})";text=read(L);start=text.rfind('            successions =',0,text.index(old));end=text.index('\n',text.index(old));block=text[start:end];new=tr(block).replace('handover_grant_id','reception_grant_id').replace(tr(old),"allowed.update({'political_assume_office_reception'})");after(L,block,'\n'+new)
    rep('tools/verify.py','TESTS = [',"TESTS = ['test_political_office_reception_panel', ")
    sys.path.insert(0,str(root/'tools'));from content.political_office_reception_links import with_political_office_reception_schema
    path='schemas/content.schema.json';files[path]=json.dumps(with_political_office_reception_schema(json.loads(read(path))),ensure_ascii=False,indent=2)+'\n'
    path='game/data/rule_capabilities.json';data=json.loads(read(path));data['capabilities'].append({'capability_id':'political_office_receptions_v1','rule_version':'political_office_receptions_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.political_office_receptions.version','equals':1}],'supported_semantics':[],'test_ids':['test_political_office_reception_panel'],'evidence':[]});files[path]=json.dumps(data,ensure_ascii=False,indent=2)+'\n'
    for path in [K+'political_office_reception_world.hpp',K+'political_office_reception_world.cpp']:before[path]='';files[path]=(root/path).read_text()
    domain=root/'native/world/src/resident/domains/politics/office_reception'
    manifest={'base':a.base,'classification':'SHARED_CANDIDATE_NOT_CENTRAL_OWNED','status':'SOURCE_ONLY_UNREGISTERED','cmake':'central sole writer','files':[],'cmake_registration':{'product_cpp':[str(x.relative_to(root)) for x in sorted(domain.glob('*.cpp'))]+[K+'political_office_reception_world.cpp'],'headers':[str(x.relative_to(root)) for x in sorted(domain.glob('*.hpp'))]+[K+'political_office_reception_world.hpp'],'existing_product_delta':['native/world/src/resident/domains/politics/political_member_history.cpp','native/world/src/resident/domains/politics/political_member_history.hpp'],'native_tests':[str(x.relative_to(root)) for x in sorted((root/'native/world/tests/resident').glob('political_office_reception*_test.cpp'))],'python_tests':['tools/test_political_office_reception_authoring.py','tools/test_political_office_reception_registration.py'],'gd_test':'game/tests/test_political_office_reception_panel.gd'}}
    def oid(text):
        """标识实际UTF8字节，输出manifest不以行数或文件名代替来源。"""
        b=text.encode();return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    patch=[]
    for path,text in sorted(files.items()):
        target=out/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(text);manifest['files'].append({'path':path,'before_oid':oid(before[path]) if before[path] else None,'after_oid':oid(text),'sha256':hashlib.sha256(text.encode()).hexdigest()});patch.extend(difflib.unified_diff(before[path].splitlines(True),text.splitlines(True),fromfile='a/'+path if before[path] else '/dev/null',tofile='b/'+path))
    (out/'candidate.patch').write_text(''.join(patch));(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps({'files':len(files),'manifest':str(out/'manifest.json')}))
if __name__=='__main__':main()

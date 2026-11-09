#!/usr/bin/env python3
"""固定Git基准生成邀请回复的共享窄候选；不写中央，不把借用外域算作owned。"""
import argparse,hashlib,json,subprocess
from pathlib import Path

def main():
 p=argparse.ArgumentParser();p.add_argument('--repo',required=True);p.add_argument('--base',required=True);p.add_argument('--output',required=True);a=p.parse_args();root=Path(a.repo);out=Path(a.output);candidate=out/'candidate';files={};before={}
 def read(path):
  if path not in files:
   s=subprocess.check_output(['git','show',a.base+':'+path],cwd=root).decode();files[path]=s;before[path]=s
  return files[path]
 def rep(path,old,new,count=1):
  s=read(path);n=s.count(old)
  if n!=count:raise RuntimeError(f'{path}: anchor {old[:90]!r}: expected {count}, got {n}')
  files[path]=s.replace(old,new)
 def after(path,old,new):rep(path,old,old+new)
 def line_after(path,needle,new):
  lines=read(path).splitlines(True);matches=[line for line in lines if needle in line]
  if len(matches)!=1:raise RuntimeError(f'{path}: line {needle!r} matches {len(matches)}')
  after(path,matches[0],new)
 K='native/world/src/resident/kernel/';M='native/world/include/hanmo/resident/model_contract.hpp';S='native/world/src/resident/model/common_model.hpp';W=K+'world_bundle.hpp'
 rep(M,'struct PoliticalGovernanceContinuationState;','struct PoliticalGovernanceContinuationState;struct GovernanceInvitationReplyState;')
 after(M,'    bool political_governance_continuations=false;','\n    bool political_governance_invitation_replies=false;')
 after(S,'    std::shared_ptr<const PoliticalGovernanceContinuationState> political_governance_continuations;','\n    std::shared_ptr<const GovernanceInvitationReplyState> governance_invitation_replies;')
 after(W,'#include "resident/domains/politics/governance_continuation/political_governance_continuation.hpp"','\n#include "governance_invitation_world.hpp"')
 rep(W,'public MembershipReleaseRead {','public MembershipReleaseRead,public GovernanceInvitationReplyRead {')
 after(W,'    explicit BundleServices(WorldBundle&);','\n    Outcome<std::optional<GovernanceInvitationReplySource>> governance_invitation_reply(const StableId&)const override;')
 after(W,'    std::optional<PoliticalGovernanceContinuationState> political_governance_continuations;','\n    std::optional<GovernanceInvitationReplyState> governance_invitation_replies;')
 # 三方合流保持原所有提交尾参数，仅末尾追加本次唯一新源。
 rep(W,'const DiscipleControlState* disciple_bindings_before=nullptr);','const DiscipleControlState* disciple_bindings_before=nullptr,const StableId& invitation_reply_source_id={});')
 B=K+'world_bundle.cpp'
 after(B,'political_governance_continuations(seed.political_governance_continuations?std::optional<PoliticalGovernanceContinuationState>{*seed.political_governance_continuations}:std::nullopt),','governance_invitation_replies(seed.governance_invitation_replies?std::optional<GovernanceInvitationReplyState>{*seed.governance_invitation_replies}:std::nullopt),')
 after(B,'political_governance_continuations(other.political_governance_continuations),','governance_invitation_replies(other.governance_invitation_replies),')
 line_after(B,'if(w.political_governance_continuations)for(', '    if(w.governance_invitation_replies)for(const auto& row:w.governance_invitation_replies->replies.rows()){const auto* h=w.common.state().facts.find(row.first);if(!w.catalog->political_governance_invitation_replies||!h)return common_failure("GovernanceInvitationFactMismatch",row.first);auto checked=validate_governance_invitation_reply_fact_header(w.common,*w.governance_invitation_replies,*h);if(!checked.ok)return checked;checked=fact(row.first,governance_invitation_reply_fact_kind,governance_invitation_reply_module);if(!checked.ok)return checked;}\n')
 line_after(B,'valid=validate_political_governance_continuation_world(*this);','    valid=validate_governance_invitation_world(*this);if(!valid.ok)return valid;\n')
 G=K+'governance_world_access.cpp'
 rep(G,'&politics_view_index,&governance_view_index};','&politics_view_index,&governance_view_index,catalog->political_governance_invitation_replies?&services:nullptr};')
 rep(G,'return release_action(*this,id);}};','return release_action(*this,id);},catalog->political_governance_invitation_replies?&services:nullptr};')
 C=K+'command_types.hpp'
 after(C,'#include "political_governance_continuation_world.hpp"','\n#include "governance_invitation_world.hpp"')
 rep(C,'ContactInquiryRequest,VisitReplyCommand>;','ContactInquiryRequest,VisitReplyCommand,GovernanceInvitationReplyCommand>;')
 rep(C,'ContactInquiryQuote,VisitReplyQuote>;','ContactInquiryQuote,VisitReplyQuote,GovernanceInvitationReplyQuote>;')
 # 完整候选自身没有NPC写入口，两个原调度入口必须在任何写前拒绝。
 P=K+'phase_plan.cpp';rep(P,'    applied=false;','    applied=false;if(std::holds_alternative<GovernanceInvitationReplyCommand>(command.data)||is_governance_invitation_reply_command(command.kind))return common_failure("GovernanceInvitationPlayerCommandRequired");')
 D=K+'dynamic_integration.cpp';after(D,'Status adopt_director_proposal(WorldBundle& w,const DecisionProposal& proposal,Integer tick){','\n    if(proposal.command&&(std::holds_alternative<GovernanceInvitationReplyCommand>(proposal.command->data)||is_governance_invitation_reply_command(proposal.command->kind)))return common_failure("GovernanceInvitationPlayerCommandRequired");')
 # 实际result在原cache/history之后才绑定；不以ID相邻猜提交来源。
 R=K+'resident_world.cpp'
 rep(R,'auto visit_reply_source=visit_reply_new_result_world(', 'StableId invitation_reply_source_id;if(is_governance_invitation_reply_command(committed_kind)){if(!current.governance_invitation_replies||!candidate->governance_invitation_replies)return {false,{},common_failure("GovernanceInvitationProfileRequired").error};auto source=governance_invitation_new_player_source(*current.governance_invitation_replies,*candidate->governance_invitation_replies,command.actor_id,committed_kind,candidate->common.tick());if(!source.ok)return {false,{},source.error};invitation_reply_source_id=std::move(source.value);}auto visit_reply_source=visit_reply_new_result_world(')
 rep(R,'current.family_disciple_legacy_enabled()?&current.family_disciple_legacy:nullptr);','current.family_disciple_legacy_enabled()?&current.family_disciple_legacy:nullptr,invitation_reply_source_id);')
 rep(R,'const DiscipleControlState* disciple_bindings_before){','const DiscipleControlState* disciple_bindings_before,const StableId& invitation_reply_source_id){')
 line_after(R,'if(!governance_continuation_id.empty()', '    if(!invitation_reply_source_id.empty()||is_governance_invitation_reply_command(kind)){if(!is_governance_invitation_reply_command(kind)||!w.catalog->political_governance_invitation_replies||!w.governance_invitation_replies||invitation_reply_source_id.empty())return common_failure("GovernanceInvitationNewPlayerSourceRequired");auto bound=bind_governance_invitation_player_result(w.common,*w.governance_invitation_replies,invitation_reply_source_id,actor,sequence,*receipt,[](const CommitResult& r){return r.details&&std::holds_alternative<Done>(r.details->data);});if(!bound.ok)return bound;}\n')
 # 完整路由先验typed变体与具名kind，实际领域仍自行复核原权源。
 D=K+'dispatcher.cpp'
 line_after(D,'if(kind==political_governance_continue_command&&!w.catalog', '    if(is_governance_invitation_reply_command(kind)&&!w.catalog->political_governance_invitation_replies)return common_failure("GovernanceInvitationProfileRequired");\n')
 line_after(D,'const auto* governance_continuation=std::get_if', '    const auto* invitation_reply=std::get_if<GovernanceInvitationReplyCommand>(&command.data);if(is_governance_invitation_reply_command(command.kind)!=(invitation_reply!=nullptr)||(invitation_reply&&command.kind!=governance_invitation_reply_command_kind(*invitation_reply)))return reject("InvalidGovernanceInvitationCommand");\n')
 line_after(D,'if(governance_continuation){auto q=', '    if(invitation_reply){auto q=prepare_governance_invitation_world(w,id,*invitation_reply);if(!q.ok)return {false,{},q.error};return ready(w,id,command,std::move(q.value));}\n')
 line_after(D,'if(const auto* c=std::get_if<PoliticalContinueGovernance>(&op.command.data))', '    if(const auto* c=std::get_if<GovernanceInvitationReplyCommand>(&op.command.data)){if(op.command.kind!=governance_invitation_reply_command_kind(*c)||!std::holds_alternative<GovernanceInvitationReplyQuote>(op.quote.data))return failed<ReceiptDetails>(common_failure("InvalidGovernanceInvitationCommand"));}else if(is_governance_invitation_reply_command(op.command.kind))return failed<ReceiptDetails>(common_failure("InvalidGovernanceInvitationCommand"));\n')
 line_after(D,'if(const auto* c=std::get_if<PoliticalContinueGovernance>(&command.data))', '    if(const auto* c=std::get_if<GovernanceInvitationReplyCommand>(&command.data)){auto r=commit_governance_invitation_world(w,id,*c,std::get<GovernanceInvitationReplyQuote>(op.quote.data));if(!r.ok)return {false,{},r.error};return {true,ReceiptDetails{Done{},{}},{}};}\n')
 # 精确Done只占自己的feature；旧receipt visitor字节分支保持。
 CB='native/world/src/resident/checkpoint/command_boundary.cpp'
 line_after(CB,'if(kind==political_governance_continue_command){boundary::fields', '    if(is_governance_invitation_reply_command(kind)){boundary::fields(value,{},{},"receipt/details");result.data=Done{};return result;}\n')
 after(CB,'Integer tag=x.data.index();c.field("variant",tag);','if(is_governance_invitation_reply_command(kind)&&(!(c.schema_features&governance_invitation_reply_feature)||tag!=0))return c.reject("InvalidGovernanceInvitationReceipt");')
 line_after(CB,'if(r.kind==political_governance_continue_command&&', '        if(is_governance_invitation_reply_command(r.kind)&&(!r.details||!std::holds_alternative<Done>(r.details->data)))failed("InvalidGovernanceInvitationReceipt","receipt/details",ErrorKind::rule);\n')
 line_after(CB,'if(value.kind==political_governance_continue_command&&!(c.schema_features', '    if(is_governance_invitation_reply_command(value.kind)&&!(c.schema_features&governance_invitation_reply_feature))return c.reject("GovernanceInvitationProfileRequired");\n')
 after(CB,'c.field("has_details",has_details);','if(is_governance_invitation_reply_command(value.kind)&&!has_details)return c.reject("InvalidGovernanceInvitationReceipt");')
 line_after(CB,'if(kind==political_governance_continue_command){auto decoded=', '    if(is_governance_invitation_reply_command(kind)){auto decoded=decode_governance_invitation_reply_command(kind,params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}\n')
 line_after(CB,'else if constexpr(std::is_same_v<T,PoliticalGovernanceContinuationQuote>)', '                else if constexpr(std::is_same_v<T,GovernanceInvitationReplyQuote>)return boundary::obj({{"ok",V{true}},{"duration",V{Integer{0}}},{"governance_invitation_replies",encode_governance_invitation_reply_quote(quote)}});\n')
 CC='native/world/src/resident/storage/common_codec.cpp'
 # 原具名JSON白名单必须双向精确，不能泛化political前缀。
 rep(CC,'const bool apprenticeship=','const bool invitation_reply=kind=="political_governance_invitation_closed";if(invitation_reply)fields(row.second,{"kind"},{"domain"},row.first);const bool apprenticeship=')
 rep(CC,'kind=="political_governance_continued"?"political_governance_continuations":','invitation_reply?"political_governance_invitation_replies":kind=="political_governance_continued"?"political_governance_continuations":')
 rep(CC,'if((kind=="political_governance_continued")!=','if(invitation_reply!=(named=="political_governance_invitation_replies"))fail("InvalidFactDomain",row.first);if((kind=="political_governance_continued")!=')
 rep(CC,'if(!(named=="political_governance_continuations"','if(!(named=="political_governance_invitation_replies"&&invitation_reply)&&!(named=="political_governance_continuations"')
 # 目录、标题及银行只在显式依赖闭合后增加本段；新尾在基准的弟子控制之后。
 WC=K+'world_codec.cpp';N=K+'native_initial_state.cpp'
 rep(WC,'if(catalog.family_disciple_legacy)domains.push_back({"family_disciple_legacy",1});return domains;','if(catalog.family_disciple_legacy)domains.push_back({"family_disciple_legacy",1});if(catalog.political_governance_invitation_replies)domains.push_back({governance_invitation_reply_module,1});return domains;')
 after(WC,'{"political_governance_continuations",{"1"}}',', {"political_governance_invitation_replies",{"1"}}')
 line_after(WC,'if(has("family_disciple_legacy")){', '        auto invitation_layout=validate_governance_invitation_reply_modules(identity.catalog.module_versions,has(governance_invitation_reply_module));if(!invitation_layout.ok)return {false,{},invitation_layout.error};if(has(governance_invitation_reply_module)){if(!dynamic)fail("GovernanceInvitationProfileRequired","checkpoint/module_versions");domains.push_back({governance_invitation_reply_module,1});}\n')
 line_after(WC,'auto continued=decode_political_governance_continuation_profile(scenario,caps);', '        auto invitation_replies=decode_governance_invitation_reply_profile(scenario,caps);if(!invitation_replies.ok)return {false,{},invitation_replies.error};result->political_governance_invitation_replies=invitation_replies.value;\n')
 line_after(WC,'if(result->family_disciple_legacy)id.module_versions', '    if(result->political_governance_invitation_replies)id.module_versions.emplace_back(governance_invitation_reply_module,"1");auto invitation_modules=validate_governance_invitation_reply_modules(id.module_versions,result->political_governance_invitation_replies);if(!invitation_modules.ok)return {false,{},invitation_modules.error};\n')
 line_after(WC,'if(catalog.political_governance_continuations){auto parsed=', '    if(catalog.political_governance_invitation_replies){auto parsed=decode_governance_invitation_replies(field(source,"political_governance_invitation_replies","state"));if(!parsed.ok)return {false,{},parsed.error};result.governance_invitation_replies=std::make_shared<const GovernanceInvitationReplyState>(std::move(parsed.value));}else if(source.find("political_governance_invitation_replies"))fail("UnexpectedGovernanceInvitationReplies","political_governance_invitation_replies");\n')
 line_after(WC,'std::set<StableId> continued_ids;', '        std::set<StableId> invitation_ids;for(const auto& row:object(field(source,"facts","state"),"facts")){const auto kind=get_string(row.second,"kind",row.first);const auto* named=row.second.find("domain");if(kind!=governance_invitation_reply_fact_kind&&(!named||string(*named,row.first)!=governance_invitation_reply_module))continue;if(!catalog.political_governance_invitation_replies||!result.governance_invitation_replies)fail("GovernanceInvitationProfileRequired",row.first);fields(row.second,{"kind","domain"},{},row.first);if(!invitation_ids.insert(row.first).second)fail("GovernanceInvitationFactMismatch",row.first);const auto* h=legal_common.state().facts.find(row.first);if(!h)fail("GovernanceInvitationFactMismatch",row.first);auto checked=validate_governance_invitation_reply_fact_header(legal_common,*result.governance_invitation_replies,*h);if(!checked.ok)return {false,{},checked.error};}if(result.governance_invitation_replies&&invitation_ids.size()!=result.governance_invitation_replies->replies.size())fail("GovernanceInvitationFactMismatch","political_governance_invitation_replies");\n')
 line_after(WC,'if(legal_header&&(legal_header->kind==political_governance_continue_kind', '        if(legal_header&&(legal_header->kind==governance_invitation_reply_fact_kind||legal_header->domain==governance_invitation_reply_module)){if(!w.catalog->political_governance_invitation_replies||!w.governance_invitation_replies||payload)fail("GovernanceInvitationFactMismatch",row.first);auto checked=validate_governance_invitation_reply_fact_header(w.common,*w.governance_invitation_replies,*legal_header);if(!checked.ok)return {false,{},checked.error};set(row.second,"domain",BoundaryValue{legal_header->domain});continue;}\n')
 line_after(WC,'if(w.catalog->political_governance_continuations){if(!w.political_governance_continuations)fail(', '    if(w.catalog->political_governance_invitation_replies){if(!w.governance_invitation_replies)fail("GovernanceInvitationProfileRequired","state");set(output,"political_governance_invitation_replies",encode_governance_invitation_replies(*w.governance_invitation_replies));}\n')
 after(WC,'CodecVisitor writer(s.payload);','if(w.catalog->political_governance_invitation_replies)writer.schema_features|=governance_invitation_reply_feature;')
 rep(WC,'CodecVisitor reader(section.payload);if(catalog.political_governance_continuations)','CodecVisitor reader(section.payload);if(catalog.political_governance_invitation_replies)reader.schema_features|=governance_invitation_reply_feature;if(catalog.political_governance_continuations)')
 after(WC,'std::uint32_t political_features=0;','{const auto& modules=decoded.value.identity.catalog.module_versions;const bool enabled=std::find(modules.begin(),modules.end(),std::make_pair(StableId{governance_invitation_reply_module},StableId{"1"}))!=modules.end();auto checked=validate_governance_invitation_reply_modules(modules,enabled);if(!checked.ok)return {false,{},checked.error};if(enabled)political_features|=governance_invitation_reply_feature;}')
 line_after(WC,'if(w.catalog->family_disciple_legacy){r=section(', '    if(w.catalog->political_governance_invitation_replies){if(!w.governance_invitation_replies)return {false,{},common_failure("GovernanceInvitationProfileRequired").error};r=section(governance_invitation_reply_module,[&](auto& c){return visit_governance_invitation_replies_checkpoint(c,*w.governance_invitation_replies);});if(!r.ok)return {false,{},r.error};}\n')
 line_after(WC,'else if(section.domain=="family_disciple_legacy"){', '        else if(section.domain==governance_invitation_reply_module){auto state=std::make_shared<GovernanceInvitationReplyState>();r=visit_governance_invitation_replies_checkpoint(reader,*state);seed.governance_invitation_replies=std::move(state);}\n')
 line_after(N,'if(const auto* config=scenario.find("family_disciple_legacy")){', '    auto invitation_replies=decode_governance_invitation_reply_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!invitation_replies.ok)return {false,{},invitation_replies.error};if(invitation_replies.value)admitted.module_versions.emplace_back(governance_invitation_reply_module,"1");auto invitation_modules=validate_governance_invitation_reply_modules(admitted.module_versions,invitation_replies.value);if(!invitation_modules.ok)return {false,{},invitation_modules.error};\n')
 line_after(N,'if(mutable_catalog->political_governance_continuations)seed.', '    if(mutable_catalog->political_governance_invitation_replies)seed.governance_invitation_replies=std::make_shared<const GovernanceInvitationReplyState>();\n')
 Q=K+'live_view_queries.cpp';line_after(Q,'if(query=="political_governance_continuations")','    if(query=="governance_invitation_replies"){auto unique=validate_economic_payload_fields(args);if(!unique.ok)return {false,{},unique.error};boundary::fields(args,{"place_id","cursor"},{},"args");return query_governance_invitation_world(world,boundary::get_string(args,"place_id","args"),boundary::get_int(args,"cursor","args"));}\n')
 # 只提供有限只读办理页；三目录producer的marker统一在弟子控制之后。
 UI='game/ui/native_resident_backend.gd'
 line_after(UI,'if definition.has("family_disciple_legacy"):', '\tif definition.get("dynamic_world",{}).has("political_governance_invitation_replies"): expected["module_versions"].append({"name":"political_governance_invitation_replies","version":"1"})\n')
 line_after(UI,'return demand_view("political_governance_continuations",', '\n## 原双方本地只读邀请回复；命令仍经原生preview。\nfunc governance_invitation_replies_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_governance_invitation_replies")\nfunc governance_invitation_replies(place_id: String, cursor: int=0) -> Dictionary:\n\treturn demand_view("governance_invitation_replies",{"place_id":place_id,"cursor":cursor})\n')
 for path in ['game/ui/resident_page_requests.gd','game/ui/screen_composer.gd']:
  line_after(path,'const GovernanceContinuationPanel=preload(', 'const GovernanceInvitationReplyPanel=preload("res://ui/governance_invitation_reply_panel.gd")\n')
 line_after('game/ui/resident_page_requests.gd','if not GovernanceContinuationPanel.prepare(host,', '\t\tif not GovernanceInvitationReplyPanel.prepare(host,str(location.get("place_id",""))): return false\n')
 line_after('game/ui/screen_composer.gd','GovernanceContinuationPanel.render(m,', '\tif not current_place.is_empty() and kernel.has_method("governance_invitation_replies_enabled") and kernel.governance_invitation_replies_enabled():\n\t\tGovernanceInvitationReplyPanel.render(m,_sidebar_section(m,stage,"GovernanceInvitationReplyArea","安民邀请回复"),kernel.governance_invitation_replies(current_place,int(m.get_meta("governance_invitation_reply_cursor",0))),current_place)\n')
 L='tools/content/links.py';line_after(L,'from .political_governance_continuation_links import','    from .political_governance_invitation_links import check_governance_invitation_replies\n')
 rep(L,'check_political_governance_continuations(pack,index)','check_political_governance_continuations(pack,index) + check_governance_invitation_replies(pack,index)')
 line_after('tools/content/dynamic_world_links.py',"allowed.add('political_continue_governance')", "            invitation_replies = dynamic.get('political_governance_invitation_replies', {})\n            if type(invitation_replies.get('version')) is int and invitation_replies['version'] == 1:\n                allowed.add('political_withdraw_governance_invitation')\n")
 rep('tools/verify.py','TESTS = [',"TESTS = ['test_governance_invitation_reply_panel', ")
 import sys
 sys.path.insert(0,str(root/'tools'))
 from content.political_governance_invitation_links import with_governance_invitation_schema
 path='schemas/content.schema.json';files[path]=json.dumps(with_governance_invitation_schema(json.loads(read(path))),ensure_ascii=False,indent=2)+'\n'
 path='game/data/rule_capabilities.json';value=json.loads(read(path));value['capabilities'].append({'capability_id':'political_governance_invitation_replies_v1','rule_version':'political_governance_invitation_replies_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.political_governance_invitation_replies.version','equals':1}],'supported_semantics':[],'test_ids':['test_governance_invitation_reply_panel'],'evidence':[]});files[path]=json.dumps(value,ensure_ascii=False,indent=2)+'\n'
 # 四kernel叶是固定S6分支上的共享候选，不纳中央owned名义。
 for path in [K+'governance_integration.hpp',K+'governance_integration.cpp',K+'governance_invitation_world.hpp',K+'governance_invitation_world.cpp']:
  if 'governance_invitation_world' in path:before[path]=''
  else:read(path)
  files[path]=subprocess.check_output(['git','show','0a3602b1714a5a9bc673cdf91625a97ef1823d16:'+path],cwd=root).decode()
 # 输出每叶精确前后OID；构建登记由中央唯一写者合入。
 manifest={'base':a.base,'status':'SOURCE_ONLY_UNREGISTERED','dependency':{'task_governance':'1163f57551bfeb83c3494c3890ef61781d9d4988'},'cmake':'central_sole_writer','files':[]}
 def oid(s):b=s.encode();return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
 for path,s in sorted(files.items()):
  dest=candidate/path;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(s);manifest['files'].append({'path':path,'before_oid':oid(before[path]) if before[path] else None,'after_oid':oid(s),'sha256':hashlib.sha256(s.encode()).hexdigest()})
 manifest['shared_kernel_source']='0a3602b1714a5a9bc673cdf91625a97ef1823d16'
 manifest['classification']='SHARED_CANDIDATE_NOT_CENTRAL_OWNED'
 manifest['runtime']='NOT_RUN; no Godot 4.7.2 executable in recovered environment'
 manifest['cmake_registration']={
  'product_cpp':[str(p.relative_to(root)) for p in sorted((root/'native/world/src/resident/domains/politics/governance_replies').glob('*.cpp'))]+[K+'governance_invitation_world.cpp'],
  'headers':[str(p.relative_to(root)) for p in sorted((root/'native/world/src/resident/domains/politics/governance_replies').glob('*.hpp'))]+[K+'governance_invitation_world.hpp'],
  'native_tests':['native/world/tests/resident/'+n for n in ['political_governance_invitation_replies_test.cpp','political_governance_invitation_receipts_test.cpp','political_governance_invitation_boundary_test.cpp']],
  'test_fixture_cpp':['native/world/tests/resident/political_governance_intents_test.cpp','native/world/src/resident/domains/politics/politics_governance_authority_test.cpp'],
  'python_tests':['tools/test_political_governance_invitation_authoring.py','tools/test_political_governance_invitation_registration.py'],
  'gd_test':'game/tests/test_governance_invitation_reply_panel.gd'}
 import difflib
 patch=[]
 for path,s in sorted(files.items()):
  patch.extend(difflib.unified_diff(before[path].splitlines(True),s.splitlines(True),fromfile='a/'+path if before[path] else '/dev/null',tofile='b/'+path))
 (out/'candidate.patch').write_text(''.join(patch))
 out.mkdir(parents=True,exist_ok=True);(out/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps({'files':len(files),'manifest':str(out/'manifest.json')}))
if __name__=='__main__':main()

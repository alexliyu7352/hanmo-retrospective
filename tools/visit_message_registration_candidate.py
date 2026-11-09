"""固定共享基准追加显式拜帖段/命令；隔离候选不改中央或在途producer。"""
import argparse, hashlib, json, subprocess
from pathlib import Path
BASE='a30c45bb3d148b12eb455602926b94408a01ad30'
ROOT='native/world/src/resident/domains/interaction/visit_messages/'
INCLUDE='#include "resident/domains/interaction/visit_messages/visit_message_world.hpp"\n'
SHARED=['native/world/src/resident/domains/interaction/interaction_model.hpp',
'native/world/src/resident/kernel/command_types.hpp','native/world/src/resident/kernel/dispatcher.cpp',
'native/world/src/resident/checkpoint/command_boundary.cpp','native/world/src/resident/kernel/phase_plan.cpp',
'native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/native_initial_state.cpp',
'native/world/src/resident/kernel/world_bundle.cpp','native/world/src/resident/storage/common_codec.cpp',
'native/world/src/resident/kernel/live_view_queries.cpp','game/ui/native_resident_backend.gd',
'game/ui/resident_page_requests.gd','game/ui/screen_composer.gd','schemas/content.schema.json',
'game/data/rule_capabilities.json','tools/content/links.py','tools/verify.py']
OWNED=[ROOT+name for name in ['visit_message.hpp','visit_message_state.hpp','visit_message_catalog.hpp','visit_message_catalog.cpp','visit_message_codec.cpp','visit_message_rules.cpp','visit_message_validation.cpp','visit_message_view.hpp','visit_message_view.cpp','visit_message_world.hpp','visit_message_world.cpp']]+['game/ui/visit_message_panel.gd','game/tests/test_visit_message_panel_guard.gd','native/world/tests/resident/visit_message_test.cpp','tools/content/visit_message_links.py','tools/test_visit_message_author.py','tools/visit_message_registration_candidate.py','tools/build_visit_message_author.py','content/packs/authored/task_contacts_1_10/pack.json','content/packs/authored/task_contacts_1_10/content.json','content/packs/authored/task_contacts_1_10/README.md','content/packs/selections/visit-messages-1.10.0.json']
KINDS='kind=="visit_message_posted"||kind=="visit_message_received"||kind=="visit_message_withdrawn"'


def replace(source,old,new):
    """唯一锚点失败即停，禁止旧整叶覆盖后继。"""
    if source.count(old)!=1: raise ValueError((old[:130],source.count(old)))
    return source.replace(old,new)


def after(source,anchor,extra):
    """按具名整行追加一次，不靠近似匹配蒙混冷尾顺序。"""
    lines=[line for line in source.splitlines() if anchor in line]
    if len(lines)!=1: raise ValueError((anchor,len(lines)))
    return replace(source,lines[0],lines[0]+'\n'+extra)


def transform(path,s):
    """旧Interaction visitor不动，独立新段在所有原冷尾后追加。"""
    if path.endswith('/interaction_model.hpp'):
        s=replace(s,'#pragma once','#pragma once\n#include "visit_messages/visit_message_state.hpp"')
        s=replace(s,'struct PartyRecruitmentConsentCatalog;','struct PartyRecruitmentConsentCatalog;\nstruct VisitMessageCatalog;')
        s=replace(s,'    std::optional<RegionalInteractionState> regional;','    std::optional<RegionalInteractionState> regional;\n    std::optional<VisitMessageState> visit_messages;')
        return replace(s,'    std::shared_ptr<const PartyRecruitmentConsentCatalog> party_recruitment_consents;','    std::shared_ptr<const PartyRecruitmentConsentCatalog> party_recruitment_consents;\n    std::shared_ptr<const VisitMessageCatalog> visit_messages;')
    if path.endswith('/command_types.hpp'):
        s=INCLUDE+s;s=replace(s,'IssueEconomicSupplyPolicyCommand,FamilyBirthCommand>;','IssueEconomicSupplyPolicyCommand,FamilyBirthCommand,VisitMessageCommand>;')
        return replace(s,'EconomicSupplyPolicyQuote,FamilyBirthQuote>;','EconomicSupplyPolicyQuote,FamilyBirthQuote,VisitMessageQuote>;')
    if path.endswith('/dispatcher.cpp'):
        s=INCLUDE+s
        s=replace(s,'Prepared prepare_command(const WorldBundle& w,const StableId& id,const CommandPayload& command){','Prepared prepare_command(const WorldBundle& w,const StableId& id,const CommandPayload& command){\n    const auto* visit=std::get_if<VisitMessageCommand>(&command.data);if(visit_message_command_kind(command.kind)!=(visit!=nullptr)||(visit&&visit_message_command_kind(*visit)!=command.kind))return reject("InvalidVisitMessageCommand");')
        s=after(s,'auto guard=command_common_guard(w,id,command.kind);','    if(visit){auto quote=prepare_visit_message_world(w,id,*visit);if(!quote.ok)return {false,{},quote.error};return ready(w,id,command,std::move(quote.value));}')
        s=after(s,'const auto* birth=std::get_if<FamilyBirthCommand>(&op.command.data);','    const auto* visit=std::get_if<VisitMessageCommand>(&op.command.data);if(visit_message_command_kind(op.command.kind)!=(visit!=nullptr)||(visit&&(visit_message_command_kind(*visit)!=op.command.kind||!std::holds_alternative<VisitMessageQuote>(op.quote.data))))return failed<ReceiptDetails>(common_failure("InvalidVisitMessageCommand"));')
        return after(s,'if(const auto* c=std::get_if<EstateEmploymentCommand>(&command.data)){auto result=', '    if(visit){auto result=commit_visit_message_world(w,id,*visit,std::get<VisitMessageQuote>(op.quote.data));if(!result.ok)return {false,{},result.error};return {true,ReceiptDetails{std::move(result.value),{}},{}};}')
    if path.endswith('/command_boundary.cpp'):
        s=INCLUDE+s
        s=after(s,'if(kind.rfind("estate_employment_",0)==0){auto decoded=', '    if(visit_message_command_kind(kind)){auto decoded=decode_visit_message_command(kind,params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}')
        s=after(s,'else if constexpr(std::is_same_v<T,FamilyBirthQuote>)return', '                else if constexpr(std::is_same_v<T,VisitMessageQuote>)return encode_visit_message_quote(quote);')
        s=replace(s,'    else if(kind=="encounter_leave") {','    else if(visit_message_command_kind(kind)){boundary::fields(value,{"settlement_result"},{},"receipt/details");InteractionEffect effect;effect.settlement_result=boundary::get_string(value,"settlement_result","receipt/details");if(effect.settlement_result.empty())failed("InvalidVisitMessageReceipt","receipt/details");result.data=std::move(effect);}\n    else if(kind=="encounter_leave") {')
        s=replace(s,'if(value.news_ids)return c.reject("UnexpectedNewsReceipt");return c.status();','if(value.news_ids)return c.reject("UnexpectedNewsReceipt");if(visit_message_command_kind(kind)&&(!value.encounter_id.empty()||value.settlement_result.empty()||value.party_id||value.person_id))return c.reject("InvalidVisitMessageReceipt");return c.status();')
        return after(s,'if(r.kind=="ask_news"){const auto* effect=', '        if(visit_message_command_kind(r.kind)){const auto* effect=r.details?std::get_if<InteractionEffect>(&r.details->data):nullptr;if(!effect||effect->settlement_result.empty()||!effect->encounter_id.empty()||effect->party_id||effect->person_id||effect->news_ids)failed("InvalidVisitMessageReceipt","receipt/details",ErrorKind::rule);}')
    if path.endswith('/phase_plan.cpp'):
        return replace(s,'auto interaction=(naval_repair_command||','auto interaction=(std::holds_alternative<VisitMessageCommand>(command.data)||naval_repair_command||')
    if path.endswith('/native_initial_state.cpp'):
        s=INCLUDE+s
        s=after(s,'admitted.module_versions.emplace_back("family_birth","1");','    auto visit_messages=decode_visit_message_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!visit_messages.ok)return {false,{},visit_messages.error};if(visit_messages.value)admitted.module_versions.emplace_back("visit_messages","1");')
        return replace(s,'auto interaction=std::make_shared<InteractionState>();','auto interaction=std::make_shared<InteractionState>();if(mutable_catalog->interaction->visit_messages)interaction->visit_messages.emplace();')
    if path.endswith('/world_codec.cpp'):
        s=INCLUDE+s
        s=replace(s,'if(family_birth_enabled(catalog))domains.push_back({"family_birth",1});return domains;','if(family_birth_enabled(catalog))domains.push_back({"family_birth",1});if(catalog.interaction&&catalog.interaction->visit_messages)domains.push_back({"visit_messages",1});return domains;')
        s=replace(s,'{"family_birth",{"1"}},','{"family_birth",{"1"}}, {"visit_messages",{"1"}},')
        s=after(s,'if(has("family_birth"))domains.push_back({"family_birth",1});','        if(has("visit_messages")){if(!dynamic)fail("VisitMessageProfileMismatch","checkpoint/module_versions");domains.push_back({"visit_messages",1});}')
        s=after(s,'if(has("family_birth","1")!=has("family_lineage","1")','            if(has("visit_messages","1")!=(version("visit_messages")==1)||(!dynamic&&version("visit_messages")))return {false,{},common_failure("VisitMessageProfileMismatch").error};')
        s=after(s,'auto recruitment=decode_party_recruitment_consent_profile(scenario,','    auto visit_messages=decode_visit_message_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!visit_messages.ok)return {false,{},visit_messages.error};')
        s=replace(s,'result->interaction=std::make_shared<const InteractionCatalog>(std::move(i.value));','if(visit_messages.value){auto decoded=decode_visit_message_catalog(field(scenario,"visit_messages","scenario"),i.value);if(!decoded.ok)return {false,{},decoded.error};i.value.visit_messages=std::make_shared<const VisitMessageCatalog>(std::move(decoded.value));}result->interaction=std::make_shared<const InteractionCatalog>(std::move(i.value));')
        s=after(s,'if(family_birth_enabled(*result)){id.module_versions','    if(result->interaction->visit_messages)id.module_versions.emplace_back("visit_messages","1");')
        s=replace(s,'result.interaction=std::make_shared<const InteractionState>(std::move(i.value));','if(catalog.interaction->visit_messages){auto decoded=decode_visit_messages(field(source,"visit_messages","state"));if(!decoded.ok)return {false,{},decoded.error};i.value.visit_messages=std::move(decoded.value);}else if(source.find("visit_messages"))fail("UnexpectedVisitMessageDomain","visit_messages");result.interaction=std::make_shared<const InteractionState>(std::move(i.value));')
        guard='(w.catalog->interaction->visit_messages&&w.interaction.visit_messages&&header->domain=="visit_messages"&&((header->kind==visit_message_posted_kind&&w.interaction.visit_messages->posted.find(row.first))||(header->kind==visit_message_received_kind&&w.interaction.visit_messages->received.find(row.first))||(header->kind==visit_message_withdrawn_kind&&w.interaction.visit_messages->withdrawn.find(row.first))))'
        s=replace(s,'||header->domain=="local_economy"||','||'+guard+'||header->domain=="local_economy"||')
        s=after(s,'set(output,"family_birth",','    if(w.catalog->interaction->visit_messages){if(!w.interaction.visit_messages)fail("MissingVisitMessageDomain","visit_messages");set(output,"visit_messages",encode_visit_messages(*w.interaction.visit_messages));}')
        s=after(s,'r=section("family_birth",','    if(w.catalog->interaction->visit_messages){if(!w.interaction.visit_messages)return {false,{},common_failure("MissingVisitMessageDomain").error};r=section("visit_messages",[&](auto& c){return visit_visit_messages_checkpoint(c,*w.interaction.visit_messages);});if(!r.ok)return {false,{},r.error};}')
        return after(s,'else if(section.domain=="family_birth")','        else if(section.domain=="visit_messages"){i->visit_messages.emplace();r=visit_visit_messages_checkpoint(reader,*i->visit_messages);}')
    if path.endswith('/world_bundle.cpp'):
        s=INCLUDE+s
        s=after(s,'auto fact=[&](const StableId& id,','    if(w.interaction.visit_messages){for(const auto& row:w.interaction.visit_messages->posted.rows()){auto r=fact(row.first,visit_message_posted_kind,"visit_messages");if(!r.ok)return r;}for(const auto& row:w.interaction.visit_messages->received.rows()){auto r=fact(row.first,visit_message_received_kind,"visit_messages");if(!r.ok)return r;}for(const auto& row:w.interaction.visit_messages->withdrawn.rows()){auto r=fact(row.first,visit_message_withdrawn_kind,"visit_messages");if(!r.ok)return r;}}')
        return after(s,'valid=validate_party_recruitment_consent_layout','    valid=validate_visit_message_world(*this);if(!valid.ok)return valid;')
    if path.endswith('/common_codec.cpp'):
        s=replace(s,'std::string domain=kind=="life_activity"','std::string domain=('+KINDS+')?"visit_messages":kind=="life_activity"')
        s=replace(s,'auto named=string(*declared,row.first);','auto named=string(*declared,row.first);if(('+KINDS+')&&named!="visit_messages")fail("InvalidFactDomain",row.first);')
        return replace(s,'if(!(named=="family_birth"','if(!(named=="visit_messages"&&('+KINDS+'))&&!(named=="family_birth"')
    if path.endswith('/live_view_queries.cpp'):
        s=INCLUDE+s
        return replace(s,'    if(query=="party_recruitment")','    if(query=="visit_messages"){boundary::fields(args,{},{"cursor","offer_cursor"},"args");return query_visit_message_world(world,args.find("cursor")?boundary::get_int(args,"cursor","args"):0,args.find("offer_cursor")?boundary::get_int(args,"offer_cursor","args"):0);}\n    if(query=="party_recruitment")')
    if path.endswith('/native_resident_backend.gd'):
        return after(s,'expected["module_versions"].append({"name":"family_birth"','\tif definition.get("visit_message_profile","")=="witnessed_reception_v1": expected["module_versions"].append({"name":"visit_messages","version":"1"})')
    if path.endswith('/resident_page_requests.gd'):
        s=after(s,'const PartyRecruitmentPanel=','const VisitMessagePanel=preload("res://ui/visit_message_panel.gd")')
        return after(s,'if not PartyRecruitmentPanel.prepare(host):','\tif not VisitMessagePanel.prepare(host): return false')
    if path.endswith('/screen_composer.gd'):
        s=after(s,'const PartyRecruitmentPanel=','const VisitMessagePanel=preload("res://ui/visit_message_panel.gd")')
        s=after(s,'PartyRecruitmentPanel.build(m,box)','\tVisitMessagePanel.build(m,box)')
        lines=[line for line in s.splitlines() if 'TaskStaticAssetPanel.visible_here(m)' in line]
        if len(lines)!=2:raise ValueError('two original sidebar locations')
        for line in lines:s=replace(s,'\n'+line+'\n','\n'+line+'\n'+line[:len(line)-len(line.lstrip())]+'if m._resident_mode and VisitMessagePanel.visible_here(m): VisitMessagePanel.build(m,_sidebar_section(m,stage,"VisitMessageArea","本人拜帖"))\n')
        return s
    if path.endswith('/content.schema.json'):
        d=json.loads(s);p=d['$defs']['scenario']['properties'];assert 'visit_message_profile' not in p;p['visit_message_profile']={'type':'string','enum':['witnessed_reception_v1']}
        fields={x:{'$ref':'#/$defs/reference_'+section} for x,section in [('sender_actor_id','actors'),('recipient_actor_id','actors'),('custodian_actor_id','actors'),('role_id','roles'),('dialogue_id','dialogues')]}
        fields.update({x:{'type':'string','minLength':1} for x in ['id','deposit_node_id','receive_node_id','withdraw_node_id']})
        p['visit_messages']={'type':'object','additionalProperties':False,'required':['version','policies'],'properties':{'version':{'type':'integer','enum':[1]},'policies':{'type':'array','minItems':1,'maxItems':4096,'items':{'type':'object','additionalProperties':False,'required':list(fields),'properties':fields}}}}
        d['properties']['required_capabilities']['items']['enum'].append('visit_messages_v1');return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/rule_capabilities.json'):
        d=json.loads(s);d['capabilities'].append({'capability_id':'visit_messages_v1','rule_version':'witnessed_reception_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.visit_message_profile','equals':'witnessed_reception_v1'}],'supported_semantics':[],'test_ids':['test_visit_message_panel_guard'],'evidence':[]});return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path=='tools/verify.py':
        return replace(s,"'test_party_recruitment_panel_guard',","'test_party_recruitment_panel_guard', 'test_visit_message_panel_guard',")
    if path=='tools/content/links.py':
        s=after(s,'from .party_recruitment_links import','    from .visit_message_links import check_visit_messages')
        return replace(s,' + check_party_recruitment_consents(pack,index)',' + check_party_recruitment_consents(pack,index) + check_visit_messages(pack,index)')
    raise ValueError(path)


def main():
    """记录每叶原OID和结果OID；共享实际写入/生产由中央串行。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--owned-repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);p.add_argument('--wip',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    oid=lambda data:hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
    head=subprocess.check_output(['git','-C',str(a.owned_repo),'rev-parse','HEAD'],text=True).strip();shared=[];owned=[]
    for path in SHARED:
        before=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);data=transform(path,before.decode()).encode();target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);shared.append(dict(path=path,base_oid=oid(before),result_oid=oid(data)))
    for path in OWNED:
        data=(a.owned_repo/path).read_bytes()
        if not a.wip:assert data==subprocess.check_output(['git','-C',str(a.owned_repo),'show',head+':'+path]),path
        target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);owned.append(dict(path=path,result_oid=oid(data)))
    (a.output/'manifest.json').write_text(json.dumps(dict(base=a.base,source_commit=head,files=shared,owned_files=owned,status='WIP' if a.wip else 'SOURCE_ONLY_RUNTIME_PENDING',build_cpp=[x for x in OWNED if x.startswith(ROOT) and x.endswith('.cpp')],build_headers=[x for x in OWNED if x.startswith(ROOT) and x.endswith('.hpp')],tests=['native/world/tests/resident/visit_message_test.cpp','game/tests/test_visit_message_panel_guard.gd'],cold_contract='Append visit_messages/1 only; old Interaction visitor/ReceiptData tag 4 unchanged. Explicit three identity producers; independent JSON/binary tail. Original Interaction savepoints include optional state; no NPC producer in v1.'),ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':main()

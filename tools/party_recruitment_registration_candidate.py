"""独立NPC本人招伴声明接入原join_party；只生成固定共享后像，不写中央。"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
BASE='56641f9a40bf5a374242eda6ded4ce65c329758e'
PREFIX='native/world/src/resident/domains/interaction/party_consent/'
INCLUDE='#include "resident/domains/interaction/party_consent/party_recruitment_consent.hpp"\n'
PATHS=['native/world/src/resident/domains/interaction/interaction_model.hpp','native/world/src/resident/domains/interaction/interaction_actions.cpp','native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/native_initial_state.cpp','native/world/src/resident/kernel/world_bundle.cpp','native/world/src/resident/kernel/live_view_queries.cpp','game/ui/native_resident_backend.gd','game/ui/resident_page_requests.gd','game/ui/screen_composer.gd','schemas/content.schema.json','game/data/rule_capabilities.json','tools/content/links.py']
OWNED=[PREFIX+x for x in ['party_recruitment_consent.hpp','party_recruitment_consent.cpp','party_recruitment_consent_codec.cpp','party_recruitment_consent_profile.cpp','party_recruitment_consent_view.cpp']]+['native/world/tests/resident/party_recruitment_consent_test.cpp','game/ui/party_recruitment_panel.gd','game/tests/test_party_recruitment_panel_guard.gd','tools/content/party_recruitment_links.py','tools/party_recruitment_registration_candidate.py','tools/test_party_recruitment_author.py','tools/build_party_recruitment_author.py','content/packs/authored/task_contacts_1_9/pack.json','content/packs/authored/task_contacts_1_9/content.json','content/packs/authored/task_contacts_1_9/README.md','content/packs/selections/party-recruitment-1.9.0.json']


def replace(source,before,after,count=1):
    """锚点不唯一即停止；不能用整旧公共头覆盖后继军事/家事。"""
    if source.count(before)!=count:raise ValueError((before,source.count(before)))
    return source.replace(before,after)


def after_line(source,anchor,added):
    """只在具名单行之后追加来源检查或同序marker。"""
    lines=[line for line in source.splitlines() if anchor in line]
    if len(lines)!=1:raise ValueError((anchor,len(lines)))
    return replace(source,lines[0],lines[0]+'\n'+added)


def transform(path,source):
    """无新Command标签、冷状态或成员账；只有原报价临时证明与严格作者配对。"""
    if path.endswith('/interaction_model.hpp'):
        source=replace(source,'struct PartyInvitationCatalog;','struct PartyInvitationCatalog;\nstruct PartyRecruitmentConsentCatalog;\nstruct PartyRecruitmentConsentQuote;')
        source=replace(source,'    std::shared_ptr<const PartyInvitationCatalog> party_invitations;','    std::shared_ptr<const PartyInvitationCatalog> party_invitations;\n    std::shared_ptr<const PartyRecruitmentConsentCatalog> party_recruitment_consents;')
        return replace(source,'std::shared_ptr<const SocialNewsQuote> news_quote; };','std::shared_ptr<const SocialNewsQuote> news_quote;std::shared_ptr<const PartyRecruitmentConsentQuote> party_recruitment_consent; };')
    if path.endswith('/interaction_actions.cpp'):
        source=INCLUDE+source
        source=replace(source,'        q.cost_per_tick=role->companion_cost_per_tick;\n        return {true,q,{}};','''        q.cost_per_tick=role->companion_cost_per_tick;
        if(catalog.party_recruitment_consents){const auto* store=dynamic_cast<const CommonStore*>(&common);if(!store)return fail<InteractionQuote>("PartyRecruitmentSourceUnavailable");auto consent=preview_party_recruitment_consent(*store,state,catalog,catalog.party_recruitment_consents.get(),access,actor,valid.value);if(!consent.ok)return {false,{},consent.error};if(consent.value){if(c.actor_id!=target)return fail<InteractionQuote>("PartyRecruitmentSourceMismatch");if(consent.value->decision!="accept")return fail<InteractionQuote>("PartyRecruitmentDeclined");q.party_recruitment_consent=std::make_shared<const PartyRecruitmentConsentQuote>(std::move(*consent.value));}}
        return {true,q,{}};''')
        return replace(source,'        if(std::holds_alternative<JoinParty>(command)){\n            PartyRecord record;','''        if(std::holds_alternative<JoinParty>(command)){
            if(catalog.party_recruitment_consents){auto fresh=preview_interaction_extra(common,state,catalog,access,actor,command);if(!fresh.ok)return {false,{},fresh.error};if(bool(fresh.value.party_recruitment_consent)!=bool(q.party_recruitment_consent))return fail<InteractionEffect>("StalePartyRecruitmentConsent");if(q.party_recruitment_consent){const auto& proof=*q.party_recruitment_consent;if(proof.source.meeting.actor_id!=actor||proof.source.speaker_actor_id!=*q.target||q.party_id!=fresh.value.party_id||q.target!=fresh.value.target||q.cost_per_tick!=fresh.value.cost_per_tick||!q.context||q.context->role_id!=proof.role_id||q.context->provider_actor_id!=proof.source.speaker_actor_id||q.context->place_id!=proof.source.meeting.context.place_id||q.context->kind!=proof.source.meeting.context.kind||q.context->encounter_id!=proof.source.meeting.context.encounter_id)return fail<InteractionEffect>("StalePartyRecruitmentConsent");auto valid=revalidate_party_recruitment_consent(common,state,catalog,catalog.party_recruitment_consents.get(),access,proof);if(!valid.ok)return {false,{},valid.error};}}
            else if(q.party_recruitment_consent)return fail<InteractionEffect>("PartyRecruitmentProfileRequired");
            PartyRecord record;''')
    if path.endswith('/native_initial_state.cpp'):
        source=INCLUDE+source
        return after_line(source,'if(invitations.value)admitted.module_versions.emplace_back', '    auto recruitment=decode_party_recruitment_consent_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!recruitment.ok)return {false,{},recruitment.error};if(recruitment.value)admitted.module_versions.emplace_back("party_recruitment_consents","1");')
    if path.endswith('/world_codec.cpp'):
        source=INCLUDE+source
        source=replace(source,'{"party_invitations",{"1"}},','{"party_invitations",{"1"}}, {"party_recruitment_consents",{"1"}},')
        source=after_line(source,'const auto party_markers=', '            const auto recruitment_markers=std::count_if(versions.begin(),versions.end(),[](const auto& value){return value.first=="party_recruitment_consents";});if(recruitment_markers>1||(recruitment_markers&&(!dynamic||!has("party_recruitment_consents","1"))))return {false,{},common_failure("PartyRecruitmentProfileMismatch").error};')
        source=after_line(source,'result->party_invitations_enabled=invitations.value;', '    auto recruitment=decode_party_recruitment_consent_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!recruitment.ok)return {false,{},recruitment.error};')
        before='result->interaction=std::make_shared<const InteractionCatalog>(std::move(i.value));'
        source=replace(source,before,'if(recruitment.value){auto decoded=decode_party_recruitment_consent_catalog(field(scenario,"party_recruitment_consents","scenario"),i.value);if(!decoded.ok)return {false,{},decoded.error};i.value.party_recruitment_consents=std::make_shared<const PartyRecruitmentConsentCatalog>(std::move(decoded.value));}'+before)
        return after_line(source,'if(result->party_invitations_enabled)id.module_versions', '    if(result->interaction->party_recruitment_consents)id.module_versions.emplace_back("party_recruitment_consents","1");')
    if path.endswith('/world_bundle.cpp'):
        source=INCLUDE+source
        return after_line(source,'if(party_invitations_enabled()){valid=validate_party_invitation_catalog', '    valid=validate_party_recruitment_consent_layout(catalog->identity.module_versions,catalog->interaction->party_recruitment_consents.get());if(!valid.ok)return valid;if(catalog->interaction->party_recruitment_consents){valid=validate_party_recruitment_consent_catalog(*catalog->interaction->party_recruitment_consents,*catalog->interaction);if(!valid.ok)return valid;}')
    if path.endswith('/live_view_queries.cpp'):
        source=INCLUDE+source
        before='    if(query=="party_participation")'
        return replace(source,before,'    if(query=="party_recruitment"){boundary::fields(args,{},{},"args");auto access=world.services.interaction_access();return query_party_recruitment_consent(world.common,world.interaction,*world.catalog->interaction,world.catalog->interaction->party_recruitment_consents.get(),access);}\n'+before)
    if path.endswith('/native_resident_backend.gd'):
        return after_line(source,'if definition.get("party_invitation_profile",', '\tif definition.get("party_recruitment_profile","")=="provider_declaration_v1": expected["module_versions"].append({"name":"party_recruitment_consents","version":"1"})')
    if path.endswith('/resident_page_requests.gd'):
        source=after_line(source,'const PartyParticipationPanel=', 'const PartyRecruitmentPanel=preload("res://ui/party_recruitment_panel.gd")')
        return after_line(source,'if not PartyParticipationPanel.prepare(host):', '\tif not PartyRecruitmentPanel.prepare(host): return false')
    if path.endswith('/screen_composer.gd'):
        source=after_line(source,'const PartyParticipationPanel=', 'const PartyRecruitmentPanel=preload("res://ui/party_recruitment_panel.gd")')
        return after_line(source,'InteractionTasksPanel.conversation(m,box,view)', '\tPartyRecruitmentPanel.build(m,box)')
    if path.endswith('/content.schema.json'):
        data=json.loads(source);properties=data['$defs']['scenario']['properties']
        assert 'party_recruitment_profile' not in properties
        properties['party_recruitment_profile']={'type':'string','enum':['provider_declaration_v1']}
        fields={x:{'$ref':'#/$defs/reference_'+section} for x,section in [('leader_actor_id','actors'),('companion_actor_id','actors'),('role_id','roles'),('dialogue_id','dialogues')]};fields.update(id={'type':'string','minLength':1},node_id={'type':'string','minLength':1},decision={'type':'string','enum':['accept','decline']})
        properties['party_recruitment_consents']={'type':'object','additionalProperties':False,'required':['version','declarations'],'properties':{'version':{'type':'integer','enum':[1]},'declarations':{'type':'array','maxItems':4096,'items':{'type':'object','additionalProperties':False,'required':list(fields),'properties':fields}}}}
        data['properties']['required_capabilities']['items']['enum'].append('party_provider_consent_v1');return json.dumps(data,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/rule_capabilities.json'):
        data=json.loads(source);data['capabilities'].append({'capability_id':'party_provider_consent_v1','rule_version':'provider_declaration_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.party_recruitment_profile','equals':'provider_declaration_v1'}],'supported_semantics':[],'test_ids':['test_interaction_tasks_prepare'],'evidence':[]});return json.dumps(data,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/links.py'):
        source=after_line(source,'from .task_static_asset_links import', '    from .party_recruitment_links import check_party_recruitment_consents')
        return replace(source,' + check_task_static_assets(pack,index)',' + check_task_static_assets(pack,index) + check_party_recruitment_consents(pack,index)')
    raise ValueError(path)


def main():
    """具名白名单保父基准/结果OID；源未冻结只能显式生成WIP，不能当完整交付。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--owned-repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);p.add_argument('--wip',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest();shared=[];owned=[];head=subprocess.check_output(['git','-C',str(a.owned_repo),'rev-parse','HEAD'],text=True).strip()
    for path in PATHS:
        original=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);result=transform(path,original.decode()).encode();target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(result);shared.append({'path':path,'base_oid':oid(original),'result_oid':oid(result)})
    for path in OWNED:
        if a.wip and not (a.owned_repo/path).is_file():continue
        result=(a.owned_repo/path).read_bytes()
        if not a.wip:assert result==subprocess.check_output(['git','-C',str(a.owned_repo),'show',head+':'+path]),path
        target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(result);owned.append({'path':path,'result_oid':oid(result)})
    (a.output/'manifest.json').write_text(json.dumps({'base':a.base,'source_commit':head,'files':shared,'owned_files':owned,'build_cpp':[x for x in OWNED if x.startswith(PREFIX) and x.endswith('.cpp')],'build_headers':[PREFIX+'party_recruitment_consent.hpp'],'tests':['native/world/tests/resident/party_recruitment_consent_test.cpp','game/tests/test_party_recruitment_panel_guard.gd'],'status':'WIP' if a.wip else 'SOURCE_ONLY_WORLD_DEFERRED','cold_contract':'No persistent state or new command tags. Original Party and Interaction visitors stay byte-identical. Three identity producers add one exact rule marker.'},ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':main()

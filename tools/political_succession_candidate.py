#!/usr/bin/env python3
"""由固定Git对象生成完整政治继位共享候选；不写中央，新增kernel也仍是隔离共享稿。"""
import argparse, hashlib, json, subprocess, difflib
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    for name in ('repo', 'base', 'output'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args(); root = Path(args.repo); out = Path(args.output)
    if (out / 'manifest.json').exists():
        raise RuntimeError('Frozen output already exists; choose a new candidate directory')
    files, before = {}, {}
    def read(path):
        if path not in files:
            before[path] = subprocess.check_output(['git', 'show', args.base + ':' + path], cwd=root).decode()
            files[path] = before[path]
        return files[path]
    def rep(path, old, new, count=1):
        value = read(path)
        if value.count(old) != count:
            raise RuntimeError(f'{path}: expected {count} anchors for {old[:110]!r}; got {value.count(old)}')
        files[path] = value.replace(old, new)
    def after(path, old, new):
        rep(path, old, old + new)
    def line_after(path, key, new):
        matches = [line for line in read(path).splitlines(True) if key in line]
        if len(matches) != 1:
            raise RuntimeError(f'{path}: expected one line for {key!r}; got {len(matches)}')
        after(path, matches[0], new)
    K = 'native/world/src/resident/kernel/'
    M = 'native/world/include/hanmo/resident/model_contract.hpp'
    S = 'native/world/src/resident/model/common_model.hpp'
    W = K + 'world_bundle.hpp'; B = K + 'world_bundle.cpp'
    after(M, 'struct GovernanceInvitationReplyState;', 'struct PoliticalSuccessionState;struct PoliticalSuccessionCatalog;')
    after(M, '    bool political_governance_invitation_replies=false;', '\n    std::shared_ptr<const PoliticalSuccessionCatalog> political_successions;')
    after(S, '    std::shared_ptr<const GovernanceInvitationReplyState> governance_invitation_replies;', '\n    std::shared_ptr<const PoliticalSuccessionState> political_successions;')
    after(W, '#include "governance_invitation_world.hpp"', '\n#include "political_succession_world.hpp"')
    after(W, '    std::optional<GovernanceInvitationReplyState> governance_invitation_replies;', '\n    std::optional<PoliticalSuccessionState> political_successions;')
    rep(W, 'const BiologicalControlState* biological_bindings_before=nullptr);', 'const BiologicalControlState* biological_bindings_before=nullptr,const StableId& political_succession_source_id={});')
    after(B, 'governance_invitation_replies(seed.governance_invitation_replies?std::optional<GovernanceInvitationReplyState>{*seed.governance_invitation_replies}:std::nullopt),', 'political_successions(seed.political_successions?std::optional<PoliticalSuccessionState>{*seed.political_successions}:std::nullopt),')
    after(B, 'governance_invitation_replies(other.governance_invitation_replies),', 'political_successions(other.political_successions),')
    line_after(B, 'valid=validate_governance_invitation_world(*this);', '    valid=validate_political_succession_world(*this);if(!valid.ok)return valid;\n')
    line_after(B, 'if(w.governance_invitation_replies)for(', '    if(w.political_successions){std::vector<StableId> ids;for(const auto& row:w.political_successions->nominations.nominations.rows())ids.push_back(row.first);for(const auto& row:w.political_successions->nominations.answers.rows())ids.push_back(row.first);for(const auto& row:w.political_successions->nominations.withdrawals.rows())ids.push_back(row.first);for(const auto& row:w.political_successions->accessions.accessions.rows())ids.push_back(row.first);for(const auto& id:ids){const auto* h=w.common.state().facts.find(id);if(!h)return common_failure("PoliticalSuccessionFactMismatch",id);auto checked=validate_political_succession_fact_header(w.common,*w.political_successions,*h);if(!checked.ok)return checked;checked=fact(id,h->kind,political_succession_domain);if(!checked.ok)return checked;}}\n')
    C = K + 'command_types.hpp'
    after(C, '#include "governance_invitation_world.hpp"', '\n#include "political_succession_world.hpp"')
    for prefix, item in [('using CommandData=', 'PoliticalSuccessionCommand'), ('using QuoteData=', 'PoliticalSuccessionQuote')]:
        line = next(line for line in read(C).splitlines(True) if line.startswith(prefix))
        rep(C, line, line.replace('>;', ',' + item + '>;'))
    # 首片只玩家producer；两NPC通路写前严格拒，不能借相同player actor绕开永久结果。
    P = K + 'phase_plan.cpp'; after(P, 'Status try_npc_command(WorldBundle& w,const StableId& actor,const CommandPayload& command,bool& applied){', '\n    applied=false;if(std::holds_alternative<PoliticalSuccessionCommand>(command.data)||is_political_succession_command(command.kind))return common_failure("PoliticalSuccessionPlayerCommandRequired");')
    D = K + 'dynamic_integration.cpp'; after(D, 'Status adopt_director_proposal(WorldBundle& w,const DecisionProposal& proposal,Integer tick){', '\n    if(proposal.command&&(std::holds_alternative<PoliticalSuccessionCommand>(proposal.command->data)||is_political_succession_command(proposal.command->kind)))return common_failure("PoliticalSuccessionPlayerCommandRequired");')
    R = K + 'resident_world.cpp'
    rep(R, 'StableId invitation_reply_source_id;', 'StableId political_succession_source_id;if(is_political_succession_command(committed_kind)){if(!current.political_successions||!candidate->political_successions)return {false,{},common_failure("PoliticalSuccessionProfileRequired").error};auto source=political_succession_new_player_source(*current.political_successions,*candidate->political_successions,command.actor_id,committed_kind,candidate->common.tick());if(!source.ok)return {false,{},source.error};political_succession_source_id=std::move(source.value);}StableId invitation_reply_source_id;')
    rep(R, 'current.family_biological_legacy_enabled()?&current.family_biological_legacy:nullptr);', 'current.family_biological_legacy_enabled()?&current.family_biological_legacy:nullptr,political_succession_source_id);')
    rep(R, 'const BiologicalControlState* biological_bindings_before){', 'const BiologicalControlState* biological_bindings_before,const StableId& political_succession_source_id){')
    line_after(R, 'auto biological_bound=bind_family_biological_result_world(', '    if(!political_succession_source_id.empty()||is_political_succession_command(kind)){auto bound=bind_political_succession_result_world(w,political_succession_source_id,actor,sequence,*receipt);if(!bound.ok)return bound;}\n')
    D = K + 'dispatcher.cpp'
    line_after(D, 'if(is_governance_invitation_reply_command(kind)&&!w.catalog', '    if(is_political_succession_command(kind)&&!w.catalog->political_successions)return common_failure("PoliticalSuccessionProfileRequired");\n')
    line_after(D, 'const auto* invitation_reply=std::get_if', '    const auto* succession=std::get_if<PoliticalSuccessionCommand>(&command.data);if(is_political_succession_command(command.kind)!=(succession!=nullptr)||(succession&&command.kind!=political_succession_command_kind(*succession)))return reject("InvalidPoliticalSuccessionCommand");\n')
    line_after(D, 'if(invitation_reply){auto q=', '    if(succession){auto q=prepare_political_succession_world(w,id,*succession);if(!q.ok)return {false,{},q.error};return ready(w,id,command,std::move(q.value));}\n')
    line_after(D, 'if(const auto* c=std::get_if<GovernanceInvitationReplyCommand>(&op.command.data))', '    if(const auto* c=std::get_if<PoliticalSuccessionCommand>(&op.command.data)){if(op.command.kind!=political_succession_command_kind(*c)||!std::holds_alternative<PoliticalSuccessionQuote>(op.quote.data))return failed<ReceiptDetails>(common_failure("InvalidPoliticalSuccessionCommand"));}else if(is_political_succession_command(op.command.kind))return failed<ReceiptDetails>(common_failure("InvalidPoliticalSuccessionCommand"));\n')
    line_after(D, 'if(const auto* c=std::get_if<GovernanceInvitationReplyCommand>(&command.data))', '    if(const auto* c=std::get_if<PoliticalSuccessionCommand>(&command.data)){auto result=commit_political_succession_world(w,id,*c,std::get<PoliticalSuccessionQuote>(op.quote.data));if(!result.ok)return {false,{},result.error};return {true,ReceiptDetails{std::move(result.value),{}},{}};}\n')
    N = K + 'native_initial_state.cpp'
    line_after(N, 'if(mutable_catalog->political_governance_invitation_replies)seed.', '    if(mutable_catalog->political_successions)seed.political_successions=std::make_shared<const PoliticalSuccessionState>();\n')
    Q = K + 'live_view_queries.cpp'
    line_after(Q, 'if(query=="governance_invitation_replies")', '    if(query=="political_successions"){auto guard=validate_economic_payload_fields(args);if(!guard.ok)return {false,{},guard.error};boundary::fields(args,{"place_id","cursor"},{},"args");return query_political_succession_world(world,boundary::get_string(args,"place_id","args"),boundary::get_int(args,"cursor","args"));}\n')
    # 原PoliticsEffects字节复用；仅已验新module版本可解四个新命令，不占旧feature位。
    V = 'native/world/src/resident/checkpoint/codec_visitor.hpp'
    after(V, '    Integer naval_codec_version=0;', '\n    // 正式继位独立module版本，仅外层已验身份赋值，不写入旧访客字节。\n    Integer political_succession_codec_version=0;')
    C = 'native/world/src/resident/checkpoint/command_boundary.cpp'
    shape = '''// 新四命令复用原政治明细，但只允许其唯一事实和正式就任原Office来源。
bool valid_political_succession_effect(const StableId& kind,const PoliticsEffects& e){if(!is_political_succession_command(kind)||!e.appointment_id.empty()||e.fact_ids.size()!=1||e.fact_ids.front().empty())return false;return kind==political_succession_accede_command?e.receipt_ids.size()==1&&!e.receipt_ids.front().empty():e.receipt_ids.empty();}
'''
    after(C, 'using V=BoundaryValue; using D=V::Dict; using A=V::Array;\n', shape)
    rep(C, 'if(is_politics_command_kind(kind)){auto effect=decode_politics_effects(value);', 'if(is_political_succession_command(kind)){auto effect=decode_politics_effects(value);if(!effect.ok)throw MarshalFailure{effect.error};if(!valid_political_succession_effect(kind,effect.value))failed("InvalidPoliticalSuccessionReceipt","receipt/details",ErrorKind::rule);result.data=std::move(effect.value);return result;}\n    if(is_politics_command_kind(kind)){auto effect=decode_politics_effects(value);')
    rep(C, 'if((is_politics_command_kind(kind)&&tag!=12)||(tag==12&&(!is_politics_command_kind(kind)||!(c.schema_features&common_checkpoint::politics_receipts))))return c.reject("InvalidPoliticsReceipt");', 'if(is_political_succession_command(kind)&&(c.political_succession_codec_version!=1||tag!=12))return c.reject("InvalidPoliticalSuccessionReceipt");if((is_politics_command_kind(kind)&&tag!=12)||(tag==12&&!((is_politics_command_kind(kind)&&(c.schema_features&common_checkpoint::politics_receipts))||(is_political_succession_command(kind)&&c.political_succession_codec_version==1))))return c.reject("InvalidPoliticsReceipt");')
    rep(C, 'else if constexpr(std::is_same_v<T,PoliticsEffects>)return visit_politics_effects(c,value);', 'else if constexpr(std::is_same_v<T,PoliticsEffects>){auto r=visit_politics_effects(c,value);if(!r.ok)return r;if(is_political_succession_command(kind)&&!valid_political_succession_effect(kind,value))return c.reject("InvalidPoliticalSuccessionReceipt");return c.status();}')
    line_after(C, 'if(is_politics_command_kind(r.kind)&&', '        if(is_political_succession_command(r.kind)&&(!r.details||!std::holds_alternative<PoliticsEffects>(r.details->data)||!valid_political_succession_effect(r.kind,std::get<PoliticsEffects>(r.details->data))))failed("InvalidPoliticalSuccessionReceipt","receipt/details",ErrorKind::rule);\n')
    line_after(C, 'if(is_governance_invitation_reply_command(value.kind)&&!(c.schema_features', '    if(is_political_succession_command(value.kind)&&c.political_succession_codec_version!=1)return c.reject("PoliticalSuccessionProfileRequired");\n')
    after(C, 'bool has_details=static_cast<bool>(value.details);c.field("has_details",has_details);', 'if(is_political_succession_command(value.kind)&&!has_details)return c.reject("MissingPoliticalSuccessionReceipt");')
    line_after(C, 'if(is_governance_invitation_reply_command(kind)){auto decoded=', '    if(is_political_succession_command(kind)){auto decoded=decode_political_succession_command(kind,params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}\n')
    line_after(C, 'else if constexpr(std::is_same_v<T,GovernanceInvitationReplyQuote>)', '                else if constexpr(std::is_same_v<T,PoliticalSuccessionQuote>)return boundary::obj({{"ok",V{true}},{"duration",V{Integer{0}}},{"political_successions",encode_political_succession_quote(quote)}});\n')
    # Common的五类新头严格对应一个新domain，JSON不得夹入第二份业务载荷。
    C = 'native/world/src/resident/storage/common_codec.cpp'
    rep(C, 'const bool invitation_reply=kind=="political_governance_invitation_closed";', 'const bool political_succession=kind=="political_successor_nominated"||kind=="political_succession_accepted"||kind=="political_succession_declined"||kind=="political_succession_withdrawn"||kind=="political_succession_acceded";if(political_succession)fields(row.second,{"kind","domain"},{},row.first);const bool invitation_reply=kind=="political_governance_invitation_closed";')
    rep(C, 'std::string domain=daily_rent?', 'std::string domain=political_succession?"political_successions":daily_rent?')
    after(C, 'auto named=string(*declared,row.first);', 'if(political_succession!=(named=="political_successions"))fail("InvalidFactDomain",row.first);')
    rep(C, 'if(!(named=="private_shop_daily_rent"&&daily_rent)', 'if(!(named=="political_successions"&&political_succession)&&!(named=="private_shop_daily_rent"&&daily_rent)')
    # 所有新尾都在cfa原biological尾之后；Common reader/writer/title亦拿同一已验module版本。
    C = K + 'world_codec.cpp'
    rep(C, 'if(catalog.family_biological_legacy)domains.push_back({"family_biological_legacy",1});return domains;', 'if(catalog.family_biological_legacy)domains.push_back({"family_biological_legacy",1});if(catalog.political_successions)domains.push_back({political_succession_domain,1});return domains;')
    after(C, '{"family_biological_legacy",{"1"}},', ' {"political_successions",{"1"}},')
    line_after(C, 'if(has("family_biological_legacy")){if(!dynamic', '        auto succession_version=political_succession_selected_version(identity.catalog.module_versions);if(!succession_version.ok)return {false,{},succession_version.error};if(succession_version.value){if(!dynamic)fail("PoliticalSuccessionProfileRequired","checkpoint/module_versions");domains.push_back({political_succession_domain,1});}\n')
    line_after(C, 'if(has("born_personal_dialogues","1")!=', '            auto succession_version=political_succession_selected_version(versions);if(!succession_version.ok)return {false,{},succession_version.error};if(succession_version.value!=version(political_succession_domain))return {false,{},common_failure("PoliticalSuccessionProfileMismatch").error};\n')
    line_after(C, 'auto invitation_replies=decode_governance_invitation_reply_profile(scenario,caps);', '        auto successions=decode_political_succession_profile(scenario,caps);if(!successions.ok)return {false,{},successions.error};if(successions.value)result->political_successions=std::make_shared<const PoliticalSuccessionCatalog>(std::move(*successions.value));\n')
    line_after(C, 'if(result->family_biological_legacy)id.module_versions', '    if(result->political_successions)id.module_versions.emplace_back(political_succession_domain,"1");auto succession_modules=political_succession_selected_version(id.module_versions);if(!succession_modules.ok)return {false,{},succession_modules.error};\n')
    line_after(C, 'if(catalog.family_biological_legacy){auto parsed=decode_biological_control_state', '    if(catalog.political_successions){auto parsed=decode_political_succession_state(field(source,political_succession_domain,"state"));if(!parsed.ok)return {false,{},parsed.error};result.political_successions=std::make_shared<const PoliticalSuccessionState>(std::move(parsed.value));}else if(source.find(political_succession_domain))fail("UnexpectedPoliticalSuccessionDomain",political_succession_domain);\n')
    line_after(C, 'std::set<StableId> invitation_ids;', '        std::set<StableId> succession_ids;for(const auto& row:object(field(source,"facts","state"),"facts")){const auto kind=get_string(row.second,"kind",row.first);const auto* named=row.second.find("domain");if(!is_political_succession_fact_kind(kind)&&(!named||string(*named,row.first)!=political_succession_domain))continue;if(!catalog.political_successions||!result.political_successions)fail("PoliticalSuccessionProfileRequired",row.first);fields(row.second,{"kind","domain"},{},row.first);if(!succession_ids.insert(row.first).second)fail("PoliticalSuccessionFactMismatch",row.first);const auto* h=legal_common.state().facts.find(row.first);if(!h)fail("PoliticalSuccessionFactMismatch",row.first);auto checked=validate_political_succession_fact_header(legal_common,*result.political_successions,*h);if(!checked.ok)return {false,{},checked.error};}if(result.political_successions&&succession_ids.size()!=result.political_successions->nominations.nominations.size()+result.political_successions->nominations.answers.size()+result.political_successions->nominations.withdrawals.size()+result.political_successions->accessions.accessions.size())fail("PoliticalSuccessionFactMismatch",political_succession_domain);\n')
    line_after(C, 'if(legal_header&&(legal_header->kind==governance_invitation_reply_fact_kind', '        if(legal_header&&(is_political_succession_fact_kind(legal_header->kind)||legal_header->domain==political_succession_domain)){if(!w.catalog->political_successions||!w.political_successions||payload)fail("PoliticalSuccessionFactMismatch",row.first);auto checked=validate_political_succession_fact_header(w.common,*w.political_successions,*legal_header);if(!checked.ok)return {false,{},checked.error};set(row.second,"domain",BoundaryValue{legal_header->domain});continue;}\n')
    line_after(C, 'if(w.catalog->family_biological_legacy)set(output,', '    if(w.catalog->political_successions){if(!w.political_successions)fail("PoliticalSuccessionProfileRequired","state");set(output,political_succession_domain,encode_political_succession_state(*w.political_successions));}\n')
    rep(C, 'CodecVisitor writer(s.payload);', 'CodecVisitor writer(s.payload);writer.political_succession_codec_version=succession_version.value;')
    # 在实际Common section分配之前核模块与catalog；不能只给独立政治尾设置版本。
    anchor = '    auto section=[&](const StableId& name,auto visit)->Status'
    rep(C, anchor, '    auto succession_version=political_succession_selected_version(w.catalog->identity.module_versions);if(!succession_version.ok)return {false,{},succession_version.error};if((succession_version.value==1)!=bool(w.catalog->political_successions))return {false,{},common_failure("PoliticalSuccessionProfileMismatch").error};\n' + anchor)
    line_after(C, 'if(w.catalog->family_biological_legacy){r=section', '    if(w.catalog->political_successions){if(!w.political_successions)return {false,{},common_failure("PoliticalSuccessionProfileRequired").error};r=section(political_succession_domain,[&](auto& c){return visit_political_succession_checkpoint(c,*w.political_successions);});if(!r.ok)return {false,{},r.error};}\n')
    line_after(C, 'auto decoded=inspect_known_checkpoint(bytes);if(!decoded.ok)', '        auto succession_version=political_succession_selected_version(decoded.value.identity.catalog.module_versions);if(!succession_version.ok)return {false,{},succession_version.error};\n')
    rep(C, 'CodecVisitor reader(section.payload);reader.schema_features=political_features;', 'CodecVisitor reader(section.payload);reader.political_succession_codec_version=succession_version.value;reader.schema_features=political_features;')
    line_after(C, 'auto decoded=decode_checkpoint(bytes,catalog.identity,domains_for(catalog));', '    auto succession_version=political_succession_selected_version(catalog.identity.module_versions);if(!succession_version.ok)return {false,{},succession_version.error};if((succession_version.value==1)!=bool(catalog.political_successions))return {false,{},common_failure("PoliticalSuccessionProfileMismatch").error};\n')
    rep(C, 'for(const auto& section:decoded.value.sections){CodecVisitor reader(section.payload);', 'for(const auto& section:decoded.value.sections){CodecVisitor reader(section.payload);reader.political_succession_codec_version=succession_version.value;')
    line_after(C, 'else if(section.domain=="family_biological_legacy")', '        else if(section.domain==political_succession_domain){auto state=std::make_shared<PoliticalSuccessionState>();r=visit_political_succession_checkpoint(reader,*state);seed.political_successions=std::move(state);}\n')
    N = K + 'native_initial_state.cpp'
    line_after(N, 'if(const auto* config=scenario.find("family_biological_legacy"))', '    auto successions=decode_political_succession_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!successions.ok)return {false,{},successions.error};if(successions.value)admitted.module_versions.emplace_back(political_succession_domain,"1");auto succession_modules=political_succession_selected_version(admitted.module_versions);if(!succession_modules.ok)return {false,{},succession_modules.error};\n')
    UI = 'game/ui/native_resident_backend.gd'
    line_after(UI, 'if definition.has("family_biological_legacy"):', '\tif definition.get("dynamic_world",{}).has("political_successions"): expected["module_versions"].append({"name":"political_successions","version":"1"})\n')
    line_after(UI, 'return demand_view("governance_invitation_replies",', '\n## 本人原首职继位的有限本地页，真实提名、接受和就任仍走原生命令。\nfunc political_successions_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_successions")\nfunc political_successions(place_id: String, cursor: int=0) -> Dictionary:\n\treturn demand_view("political_successions",{"place_id":place_id,"cursor":cursor})\n')
    for path in ['game/ui/resident_page_requests.gd','game/ui/screen_composer.gd']:
        line_after(path, 'const GovernanceInvitationReplyPanel=preload(', 'const PoliticalSuccessionPanel=preload("res://ui/political_succession_panel.gd")\n')
    line_after('game/ui/resident_page_requests.gd', 'if not GovernanceInvitationReplyPanel.prepare(host,', '\t\tif not PoliticalSuccessionPanel.prepare(host,str(location.get("place_id",""))): return false\n')
    line_after('game/ui/screen_composer.gd', 'GovernanceInvitationReplyPanel.render(m,', '\tif not current_place.is_empty() and kernel.has_method("political_successions_enabled") and kernel.political_successions_enabled():\n\t\tPoliticalSuccessionPanel.render(m,_sidebar_section(m,stage,"PoliticalSuccessionArea","原首职正式继位"),kernel.political_successions(current_place,int(m.get_meta("political_succession_cursor",0))),current_place)\n')
    L = 'tools/content/links.py'
    line_after(L, 'from .political_governance_invitation_links import', '    from .political_succession_links import check_political_successions\n')
    rep(L, 'check_governance_invitation_replies(pack,index)', 'check_governance_invitation_replies(pack,index) + check_political_successions(pack,index)')
    line_after('tools/content/dynamic_world_links.py', "allowed.add('political_withdraw_governance_invitation')", "            successions = dynamic.get('political_successions', {})\n            if type(successions.get('version')) is int and successions['version'] == 1:\n                allowed.update({'political_nominate_successor', 'political_withdraw_successor'})\n")
    rep('tools/verify.py', 'TESTS = [', "TESTS = ['test_political_succession_panel', ")
    import sys
    sys.path.insert(0,str(root/'tools'))
    from content.political_succession_links import with_political_succession_schema
    path='schemas/content.schema.json'; files[path]=json.dumps(with_political_succession_schema(json.loads(read(path))),ensure_ascii=False,indent=2)+'\n'
    path='game/data/rule_capabilities.json'; value=json.loads(read(path))
    value['capabilities'].append({'capability_id':'political_successions_v1','rule_version':'political_successions_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.political_successions.version','equals':1}],'supported_semantics':[],'test_ids':['test_political_succession_panel'],'evidence':[]})
    files[path]=json.dumps(value,ensure_ascii=False,indent=2)+'\n'
    for path in [K + 'political_succession_world.hpp', K + 'political_succession_world.cpp']:
        before[path] = ''; files[path] = (root / path).read_text()
    # 当前只冻结可编接头，完整三个producer/codec/UI随后同一脚本补全才可交中央启用。
    manifest = {'base': args.base, 'classification': 'SHARED_CANDIDATE_NOT_CENTRAL_OWNED',
                'status': 'SOURCE_ONLY_UNREGISTERED', 'remaining': ['World/behavior/Godot runtime not run', 'full resource compose blocked by recovered asset gap'],
                'cmake': 'central sole writer', 'files': []}
    manifest['dependencies']=[{'sha':'e38bc52b230d3664082da1bd992f243da4a278a0','path':'native/world/src/resident/domains/tasks/message_notices/task_message_notice_history.hpp','oid':'63d0dc3e055e5f3d36513f53d8722a4a6ab3ea76','classification':'S3_OWNED_DEPENDENCY'}]
    domain=root/'native/world/src/resident/domains/politics/succession'
    manifest['cmake_registration']={'product_cpp':[str(p.relative_to(root)) for p in sorted(domain.glob('*.cpp'))]+[K+'political_succession_world.cpp'],'headers':[str(p.relative_to(root)) for p in sorted(domain.glob('*.hpp'))]+[K+'political_succession_world.hpp',manifest['dependencies'][0]['path']],'existing_product_delta':['native/world/src/resident/domains/politics/political_member_history.cpp','native/world/src/resident/domains/politics/political_member_history.hpp'],'native_tests':[str(p.relative_to(root)) for p in sorted((root/'native/world/tests/resident').glob('political_succession*_test.cpp'))],'python_tests':['tools/test_political_succession_authoring.py','tools/test_political_succession_registration.py'],'gd_test':'game/tests/test_political_succession_panel.gd'}
    def oid(text):
        data = text.encode(); return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
    patch = []
    for path, text in sorted(files.items()):
        target = out / 'candidate' / path; target.parent.mkdir(parents=True, exist_ok=True); target.write_text(text)
        manifest['files'].append({'path': path, 'before_oid': oid(before[path]) if before[path] else None,
                                  'after_oid': oid(text), 'sha256': hashlib.sha256(text.encode()).hexdigest()})
        patch.extend(difflib.unified_diff(before[path].splitlines(True), text.splitlines(True), fromfile='a/' + path if before[path] else '/dev/null', tofile='b/' + path))
    (out / 'candidate.patch').write_text(''.join(patch)); (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({'files': len(files), 'manifest': str(out / 'manifest.json')}))


if __name__ == '__main__':
    main()

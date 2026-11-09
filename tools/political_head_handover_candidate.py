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
    after(M, 'struct GovernanceInvitationReplyState;', 'struct PoliticalHeadHandoverState;struct PoliticalHeadHandoverCatalog;')
    after(M, '    bool political_governance_invitation_replies=false;', '\n    std::shared_ptr<const PoliticalHeadHandoverCatalog> political_head_handovers;')
    after(S, '    std::shared_ptr<const GovernanceInvitationReplyState> governance_invitation_replies;', '\n    std::shared_ptr<const PoliticalHeadHandoverState> political_head_handovers;')
    after(W, '#include "governance_invitation_world.hpp"', '\n#include "political_head_handover_world.hpp"')
    after(W, '    std::optional<GovernanceInvitationReplyState> governance_invitation_replies;', '\n    std::optional<PoliticalHeadHandoverState> political_head_handovers;')
    rep(W, 'const StableId& political_succession_source_id={});', 'const StableId& political_succession_source_id={},const StableId& political_head_handover_source_id={});')
    after(B, 'governance_invitation_replies(seed.governance_invitation_replies?std::optional<GovernanceInvitationReplyState>{*seed.governance_invitation_replies}:std::nullopt),', 'political_head_handovers(seed.political_head_handovers?std::optional<PoliticalHeadHandoverState>{*seed.political_head_handovers}:std::nullopt),')
    after(B, 'governance_invitation_replies(other.governance_invitation_replies),', 'political_head_handovers(other.political_head_handovers),')
    line_after(B, 'valid=validate_political_succession_world(*this);', '    valid=validate_political_head_handover_world(*this);if(!valid.ok)return valid;\n')
    line_after(B, 'if(w.governance_invitation_replies)for(', '    if(w.political_head_handovers){std::vector<StableId> ids;for(const auto& row:w.political_head_handovers->offers.rows())ids.push_back(row.first);for(const auto& row:w.political_head_handovers->answers.rows())ids.push_back(row.first);for(const auto& row:w.political_head_handovers->withdrawals.rows())ids.push_back(row.first);for(const auto& id:ids){const auto* h=w.common.state().facts.find(id);if(!h)return common_failure("PoliticalHeadHandoverFactMismatch",id);auto checked=validate_political_head_handover_fact_header(w.common,*w.political_head_handovers,*h);if(!checked.ok)return checked;checked=fact(id,h->kind,political_head_handover_domain);if(!checked.ok)return checked;}}\n')
    C = K + 'command_types.hpp'
    after(C, '#include "governance_invitation_world.hpp"', '\n#include "political_head_handover_world.hpp"')
    for prefix, item in [('using CommandData=', 'PoliticalHeadHandoverCommand'), ('using QuoteData=', 'PoliticalHeadHandoverQuote')]:
        line = next(line for line in read(C).splitlines(True) if line.startswith(prefix))
        rep(C, line, line.replace('>;', ',' + item + '>;'))
    # 首片只玩家producer；两NPC通路写前严格拒，不能借相同player actor绕开永久结果。
    P = K + 'phase_plan.cpp'; after(P, 'Status try_npc_command(WorldBundle& w,const StableId& actor,const CommandPayload& command,bool& applied){', '\n    applied=false;if(std::holds_alternative<PoliticalHeadHandoverCommand>(command.data)||is_political_head_handover_command(command.kind))return common_failure("PoliticalHeadHandoverPlayerCommandRequired");')
    D = K + 'dynamic_integration.cpp'; after(D, 'Status adopt_director_proposal(WorldBundle& w,const DecisionProposal& proposal,Integer tick){', '\n    if(proposal.command&&(std::holds_alternative<PoliticalHeadHandoverCommand>(proposal.command->data)||is_political_head_handover_command(proposal.command->kind)))return common_failure("PoliticalHeadHandoverPlayerCommandRequired");')
    R = K + 'resident_world.cpp'
    rep(R, 'StableId invitation_reply_source_id;', 'StableId political_head_handover_source_id;if(is_political_head_handover_command(committed_kind)){if(!current.political_head_handovers||!candidate->political_head_handovers)return {false,{},common_failure("PoliticalHeadHandoverProfileRequired").error};auto source=political_head_handover_new_player_source(*current.political_head_handovers,*candidate->political_head_handovers,command.actor_id,committed_kind,candidate->common.tick());if(!source.ok)return {false,{},source.error};political_head_handover_source_id=std::move(source.value);}StableId invitation_reply_source_id;')
    rep(R, 'political_succession_source_id);', 'political_succession_source_id,political_head_handover_source_id);')
    rep(R, 'const StableId& political_succession_source_id){', 'const StableId& political_succession_source_id,const StableId& political_head_handover_source_id){')
    line_after(R, 'auto bound=bind_political_succession_result_world(', '    if(!political_head_handover_source_id.empty()||is_political_head_handover_command(kind)){auto bound=bind_political_head_handover_result_world(w,political_head_handover_source_id,actor,sequence,*receipt);if(!bound.ok)return bound;}\n')
    D = K + 'dispatcher.cpp'
    line_after(D, 'if(is_governance_invitation_reply_command(kind)&&!w.catalog', '    if(is_political_head_handover_command(kind)&&!w.catalog->political_head_handovers)return common_failure("PoliticalHeadHandoverProfileRequired");\n')
    line_after(D, 'const auto* invitation_reply=std::get_if', '    const auto* handover=std::get_if<PoliticalHeadHandoverCommand>(&command.data);if(is_political_head_handover_command(command.kind)!=(handover!=nullptr)||(handover&&command.kind!=political_head_handover_command_kind(*handover)))return reject("InvalidPoliticalHeadHandoverCommand");\n')
    line_after(D, 'if(invitation_reply){auto q=', '    if(handover){auto q=prepare_political_head_handover_world(w,id,*handover);if(!q.ok)return {false,{},q.error};return ready(w,id,command,std::move(q.value));}\n')
    line_after(D, 'if(const auto* c=std::get_if<GovernanceInvitationReplyCommand>(&op.command.data))', '    if(const auto* c=std::get_if<PoliticalHeadHandoverCommand>(&op.command.data)){if(op.command.kind!=political_head_handover_command_kind(*c)||!std::holds_alternative<PoliticalHeadHandoverQuote>(op.quote.data))return failed<ReceiptDetails>(common_failure("InvalidPoliticalHeadHandoverCommand"));}else if(is_political_head_handover_command(op.command.kind))return failed<ReceiptDetails>(common_failure("InvalidPoliticalHeadHandoverCommand"));\n')
    line_after(D, 'if(const auto* c=std::get_if<GovernanceInvitationReplyCommand>(&command.data))', '    if(const auto* c=std::get_if<PoliticalHeadHandoverCommand>(&command.data)){auto result=commit_political_head_handover_world(w,id,*c,std::get<PoliticalHeadHandoverQuote>(op.quote.data));if(!result.ok)return {false,{},result.error};return {true,ReceiptDetails{std::move(result.value),{}},{}};}\n')
    N = K + 'native_initial_state.cpp'
    line_after(N, 'if(mutable_catalog->political_governance_invitation_replies)seed.', '    if(mutable_catalog->political_head_handovers)seed.political_head_handovers=std::make_shared<const PoliticalHeadHandoverState>();\n')
    Q = K + 'live_view_queries.cpp'
    line_after(Q, 'if(query=="governance_invitation_replies")', '    if(query=="political_head_handovers"){auto guard=validate_economic_payload_fields(args);if(!guard.ok)return {false,{},guard.error};boundary::fields(args,{"place_id","cursor"},{},"args");return query_political_head_handover_world(world,boundary::get_string(args,"place_id","args"),boundary::get_int(args,"cursor","args"));}\n')
    # 原PoliticsEffects字节复用；仅已验新module版本可解三个新命令，不占旧feature位。
    V = 'native/world/src/resident/checkpoint/codec_visitor.hpp'
    after(V, '    Integer naval_codec_version=0;', '\n    // 正式继位独立module版本，仅外层已验身份赋值，不写入旧访客字节。\n    Integer political_head_handover_codec_version=0;')
    C = 'native/world/src/resident/checkpoint/command_boundary.cpp'
    shape = ''
    after(C, 'using V=BoundaryValue; using D=V::Dict; using A=V::Array;\n', shape)
    rep(C, 'if(is_politics_command_kind(kind)){auto effect=decode_politics_effects(value);', 'if(is_political_head_handover_command(kind)){auto effect=decode_politics_effects(value);if(!effect.ok)throw MarshalFailure{effect.error};if(!valid_political_head_handover_effect(kind,effect.value))failed("InvalidPoliticalHeadHandoverReceipt","receipt/details",ErrorKind::rule);result.data=std::move(effect.value);return result;}\n    if(is_politics_command_kind(kind)){auto effect=decode_politics_effects(value);')
    rep(C, 'if((is_politics_command_kind(kind)&&tag!=12)||(tag==12&&!((is_politics_command_kind(kind)&&(c.schema_features&common_checkpoint::politics_receipts))||(is_political_succession_command(kind)&&c.political_succession_codec_version==1))))return c.reject("InvalidPoliticsReceipt");', 'if(is_political_head_handover_command(kind)&&(c.political_head_handover_codec_version!=1||tag!=12))return c.reject("InvalidPoliticalHeadHandoverReceipt");if((is_politics_command_kind(kind)&&tag!=12)||(tag==12&&!((is_politics_command_kind(kind)&&(c.schema_features&common_checkpoint::politics_receipts))||(is_political_head_handover_command(kind)&&c.political_head_handover_codec_version==1)||(is_political_succession_command(kind)&&c.political_succession_codec_version==1))))return c.reject("InvalidPoliticsReceipt");')
    rep(C, 'if(is_political_succession_command(kind)&&!valid_political_succession_effect(kind,value))return c.reject("InvalidPoliticalSuccessionReceipt");', 'if(is_political_succession_command(kind)&&!valid_political_succession_effect(kind,value))return c.reject("InvalidPoliticalSuccessionReceipt");if(is_political_head_handover_command(kind)&&!valid_political_head_handover_effect(kind,value))return c.reject("InvalidPoliticalHeadHandoverReceipt");')
    line_after(C, 'if(is_politics_command_kind(r.kind)&&', '        if(is_political_head_handover_command(r.kind)&&(!r.details||!std::holds_alternative<PoliticsEffects>(r.details->data)||!valid_political_head_handover_effect(r.kind,std::get<PoliticsEffects>(r.details->data))))failed("InvalidPoliticalHeadHandoverReceipt","receipt/details",ErrorKind::rule);\n')
    line_after(C, 'if(is_governance_invitation_reply_command(value.kind)&&!(c.schema_features', '    if(is_political_head_handover_command(value.kind)&&c.political_head_handover_codec_version!=1)return c.reject("PoliticalHeadHandoverProfileRequired");\n')
    after(C, 'bool has_details=static_cast<bool>(value.details);c.field("has_details",has_details);', 'if(is_political_head_handover_command(value.kind)&&!has_details)return c.reject("MissingPoliticalHeadHandoverReceipt");')
    line_after(C, 'if(is_governance_invitation_reply_command(kind)){auto decoded=', '    if(is_political_head_handover_command(kind)){auto decoded=decode_political_head_handover_command(kind,params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}\n')
    line_after(C, 'else if constexpr(std::is_same_v<T,GovernanceInvitationReplyQuote>)', '                else if constexpr(std::is_same_v<T,PoliticalHeadHandoverQuote>)return boundary::obj({{"ok",V{true}},{"duration",V{Integer{0}}},{"political_head_handovers",encode_political_head_handover_quote(quote)}});\n')
    # Common的五类新头严格对应一个新domain，JSON不得夹入第二份业务载荷。
    C = 'native/world/src/resident/storage/common_codec.cpp'
    rep(C, 'const bool invitation_reply=kind=="political_governance_invitation_closed";', 'const bool political_head_handover=kind=="political_head_handover_offered"||kind=="political_head_handover_accepted"||kind=="political_head_handover_declined"||kind=="political_head_handover_withdrawn";if(political_head_handover)fields(row.second,{"kind","domain"},{},row.first);const bool invitation_reply=kind=="political_governance_invitation_closed";')
    rep(C, 'std::string domain=political_succession?', 'std::string domain=political_head_handover?"political_head_handovers":political_succession?')
    after(C, 'auto named=string(*declared,row.first);', 'if(political_head_handover!=(named=="political_head_handovers"))fail("InvalidFactDomain",row.first);')
    rep(C, 'if(!(named=="political_successions"&&political_succession)', 'if(!(named=="political_head_handovers"&&political_head_handover)&&!(named=="political_successions"&&political_succession)')
    # 所有新尾都在cfa原biological尾之后；Common reader/writer/title亦拿同一已验module版本。
    C = K + 'world_codec.cpp'
    rep(C, 'if(catalog.political_successions)domains.push_back({political_succession_domain,1});return domains;', 'if(catalog.political_successions)domains.push_back({political_succession_domain,1});if(catalog.political_head_handovers)domains.push_back({political_head_handover_domain,1});return domains;')
    after(C, '{"family_biological_legacy",{"1"}},', ' {"political_head_handovers",{"1"}},')
    line_after(C, 'auto succession_version=political_succession_selected_version(identity.catalog.module_versions)', '        auto handover_version=political_head_handover_selected_version(identity.catalog.module_versions);if(!handover_version.ok)return {false,{},handover_version.error};if(handover_version.value){if(!dynamic)fail("PoliticalHeadHandoverProfileRequired","checkpoint/module_versions");domains.push_back({political_head_handover_domain,1});}\n')
    line_after(C, 'if(has("born_personal_dialogues","1")!=', '            auto handover_version=political_head_handover_selected_version(versions);if(!handover_version.ok)return {false,{},handover_version.error};if(handover_version.value!=version(political_head_handover_domain))return {false,{},common_failure("PoliticalHeadHandoverProfileMismatch").error};\n')
    line_after(C, 'auto invitation_replies=decode_governance_invitation_reply_profile(scenario,caps);', '        auto handovers=decode_political_head_handover_profile(scenario,caps);if(!handovers.ok)return {false,{},handovers.error};if(handovers.value)result->political_head_handovers=std::make_shared<const PoliticalHeadHandoverCatalog>(std::move(*handovers.value));\n')
    line_after(C, 'if(result->political_successions)id.module_versions', '    if(result->political_head_handovers)id.module_versions.emplace_back(political_head_handover_domain,"1");auto handover_modules=political_head_handover_selected_version(id.module_versions);if(!handover_modules.ok)return {false,{},handover_modules.error};\n')
    line_after(C, 'if(catalog.political_successions){auto parsed=decode_political_succession_state', '    if(catalog.political_head_handovers){auto parsed=decode_political_head_handover_state(field(source,political_head_handover_domain,"state"));if(!parsed.ok)return {false,{},parsed.error};result.political_head_handovers=std::make_shared<const PoliticalHeadHandoverState>(std::move(parsed.value));}else if(source.find(political_head_handover_domain))fail("UnexpectedPoliticalHeadHandoverDomain",political_head_handover_domain);\n')
    line_after(C, 'std::set<StableId> invitation_ids;', '        std::set<StableId> handover_ids;for(const auto& row:object(field(source,"facts","state"),"facts")){const auto kind=get_string(row.second,"kind",row.first);const auto* named=row.second.find("domain");if(!is_political_head_handover_fact_kind(kind)&&(!named||string(*named,row.first)!=political_head_handover_domain))continue;if(!catalog.political_head_handovers||!result.political_head_handovers)fail("PoliticalHeadHandoverProfileRequired",row.first);fields(row.second,{"kind","domain"},{},row.first);if(!handover_ids.insert(row.first).second)fail("PoliticalHeadHandoverFactMismatch",row.first);const auto* h=legal_common.state().facts.find(row.first);if(!h)fail("PoliticalHeadHandoverFactMismatch",row.first);auto checked=validate_political_head_handover_fact_header(legal_common,*result.political_head_handovers,*h);if(!checked.ok)return {false,{},checked.error};}if(result.political_head_handovers&&handover_ids.size()!=result.political_head_handovers->offers.size()+result.political_head_handovers->answers.size()+result.political_head_handovers->withdrawals.size())fail("PoliticalHeadHandoverFactMismatch",political_head_handover_domain);\n')
    line_after(C, 'if(legal_header&&(legal_header->kind==governance_invitation_reply_fact_kind', '        if(legal_header&&(is_political_head_handover_fact_kind(legal_header->kind)||legal_header->domain==political_head_handover_domain)){if(!w.catalog->political_head_handovers||!w.political_head_handovers||payload)fail("PoliticalHeadHandoverFactMismatch",row.first);auto checked=validate_political_head_handover_fact_header(w.common,*w.political_head_handovers,*legal_header);if(!checked.ok)return {false,{},checked.error};set(row.second,"domain",BoundaryValue{legal_header->domain});continue;}\n')
    line_after(C, 'if(w.catalog->political_successions){if(!w.political_successions)fail(', '    if(w.catalog->political_head_handovers){if(!w.political_head_handovers)fail("PoliticalHeadHandoverProfileRequired","state");set(output,political_head_handover_domain,encode_political_head_handover_state(*w.political_head_handovers));}\n')
    rep(C, 'CodecVisitor writer(s.payload);', 'CodecVisitor writer(s.payload);writer.political_head_handover_codec_version=handover_version.value;')
    # 在实际Common section分配之前核模块与catalog；不能只给独立政治尾设置版本。
    anchor = '    auto section=[&](const StableId& name,auto visit)->Status'
    rep(C, anchor, '    auto handover_version=political_head_handover_selected_version(w.catalog->identity.module_versions);if(!handover_version.ok)return {false,{},handover_version.error};if((handover_version.value==1)!=bool(w.catalog->political_head_handovers))return {false,{},common_failure("PoliticalHeadHandoverProfileMismatch").error};\n' + anchor)
    line_after(C, 'if(w.catalog->political_successions){if(!w.political_successions)return', '    if(w.catalog->political_head_handovers){if(!w.political_head_handovers)return {false,{},common_failure("PoliticalHeadHandoverProfileRequired").error};r=section(political_head_handover_domain,[&](auto& c){return visit_political_head_handover_checkpoint(c,*w.political_head_handovers);});if(!r.ok)return {false,{},r.error};}\n')
    line_after(C, 'auto decoded=inspect_known_checkpoint(bytes);if(!decoded.ok)', '        auto handover_version=political_head_handover_selected_version(decoded.value.identity.catalog.module_versions);if(!handover_version.ok)return {false,{},handover_version.error};\n')
    rep(C, 'CodecVisitor reader(section.payload);reader.political_succession_codec_version=succession_version.value;reader.schema_features=political_features;', 'CodecVisitor reader(section.payload);reader.political_succession_codec_version=succession_version.value;reader.political_head_handover_codec_version=handover_version.value;reader.schema_features=political_features;')
    line_after(C, 'auto decoded=decode_checkpoint(bytes,catalog.identity,domains_for(catalog));', '    auto handover_version=political_head_handover_selected_version(catalog.identity.module_versions);if(!handover_version.ok)return {false,{},handover_version.error};if((handover_version.value==1)!=bool(catalog.political_head_handovers))return {false,{},common_failure("PoliticalHeadHandoverProfileMismatch").error};\n')
    rep(C, 'for(const auto& section:decoded.value.sections){CodecVisitor reader(section.payload);', 'for(const auto& section:decoded.value.sections){CodecVisitor reader(section.payload);reader.political_head_handover_codec_version=handover_version.value;')
    line_after(C, 'else if(section.domain==political_succession_domain)', '        else if(section.domain==political_head_handover_domain){auto state=std::make_shared<PoliticalHeadHandoverState>();r=visit_political_head_handover_checkpoint(reader,*state);seed.political_head_handovers=std::move(state);}\n')
    N = K + 'native_initial_state.cpp'
    line_after(N, 'auto successions=decode_political_succession_profile(', '    auto handovers=decode_political_head_handover_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!handovers.ok)return {false,{},handovers.error};if(handovers.value)admitted.module_versions.emplace_back(political_head_handover_domain,"1");auto handover_modules=political_head_handover_selected_version(admitted.module_versions);if(!handover_modules.ok)return {false,{},handover_modules.error};\n')
    UI = 'game/ui/native_resident_backend.gd'
    line_after(UI, 'if definition.get("dynamic_world",{}).has("political_successions"):', '\tif definition.get("dynamic_world",{}).has("political_head_handovers"): expected["module_versions"].append({"name":"political_head_handovers","version":"1"})\n')
    line_after(UI, 'return demand_view("governance_invitation_replies",', '\n## 本人原首职继位的有限本地页，真实提名、接受和就任仍走原生命令。\nfunc political_head_handovers_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_head_handovers")\nfunc political_head_handovers(place_id: String, cursor: int=0) -> Dictionary:\n\treturn demand_view("political_head_handovers",{"place_id":place_id,"cursor":cursor})\n')
    for path in ['game/ui/resident_page_requests.gd','game/ui/screen_composer.gd']:
        line_after(path, 'const GovernanceInvitationReplyPanel=preload(', 'const PoliticalHeadHandoverPanel=preload("res://ui/political_head_handover_panel.gd")\n')
    line_after('game/ui/resident_page_requests.gd', 'if not GovernanceInvitationReplyPanel.prepare(host,', '\t\tif not PoliticalHeadHandoverPanel.prepare(host,str(location.get("place_id",""))): return false\n')
    line_after('game/ui/screen_composer.gd', 'GovernanceInvitationReplyPanel.render(m,', '\tif not current_place.is_empty() and kernel.has_method("political_head_handovers_enabled") and kernel.political_head_handovers_enabled():\n\t\tPoliticalHeadHandoverPanel.render(m,_sidebar_section(m,stage,"PoliticalHeadHandoverArea","原首职生前交接"),kernel.political_head_handovers(current_place,int(m.get_meta("political_head_handover_cursor",0))),current_place)\n')
    L = 'tools/content/links.py'
    line_after(L, 'from .political_governance_invitation_links import', '    from .political_head_handover_links import check_political_head_handovers\n')
    rep(L, 'check_governance_invitation_replies(pack,index)', 'check_governance_invitation_replies(pack,index) + check_political_head_handovers(pack,index)')
    line_after('tools/content/dynamic_world_links.py', "allowed.add('political_withdraw_governance_invitation')", "            successions = dynamic.get('political_head_handovers', {})\n            if type(successions.get('version')) is int and successions['version'] == 1:\n                allowed.update({'political_offer_head_handover', 'political_withdraw_head_handover'})\n")
    rep('tools/verify.py', 'TESTS = [', "TESTS = ['test_political_head_handover_panel', ")
    import sys
    sys.path.insert(0,str(root/'tools'))
    from content.political_head_handover_links import with_political_head_handover_schema
    path='schemas/content.schema.json'; files[path]=json.dumps(with_political_head_handover_schema(json.loads(read(path))),ensure_ascii=False,indent=2)+'\n'
    path='game/data/rule_capabilities.json'; value=json.loads(read(path))
    value['capabilities'].append({'capability_id':'political_head_handovers_v1','rule_version':'political_head_handovers_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.political_head_handovers.version','equals':1}],'supported_semantics':[],'test_ids':['test_political_head_handover_panel'],'evidence':[]})
    files[path]=json.dumps(value,ensure_ascii=False,indent=2)+'\n'
    for path in [K + 'political_head_handover_world.hpp', K + 'political_head_handover_world.cpp']:
        before[path] = ''; files[path] = (root / path).read_text()
    # 当前只冻结可编接头，完整三个producer/codec/UI随后同一脚本补全才可交中央启用。
    manifest = {'base': args.base, 'classification': 'SHARED_CANDIDATE_NOT_CENTRAL_OWNED',
                'status': 'SOURCE_ONLY_UNREGISTERED', 'remaining': ['World/behavior/Godot runtime not run', 'full resource compose blocked by recovered asset gap'],
                'cmake': 'central sole writer', 'files': []}
    manifest['dependencies']=[]
    domain=root/'native/world/src/resident/domains/politics/head_handover'
    manifest['cmake_registration']={'product_cpp':[str(p.relative_to(root)) for p in sorted(domain.glob('*.cpp'))]+[K+'political_head_handover_world.cpp'],'headers':[str(p.relative_to(root)) for p in sorted(domain.glob('*.hpp'))]+[K+'political_head_handover_world.hpp'],'existing_product_delta':['native/world/src/resident/domains/politics/political_member_history.cpp','native/world/src/resident/domains/politics/political_member_history.hpp'],'native_tests':[str(p.relative_to(root)) for p in sorted((root/'native/world/tests/resident').glob('political_head_handover*_test.cpp'))],'python_tests':['tools/test_political_head_handover_authoring.py','tools/test_political_head_handover_registration.py'],'gd_test':'game/tests/test_political_head_handover_panel.gd'}
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

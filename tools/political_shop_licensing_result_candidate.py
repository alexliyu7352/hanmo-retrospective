#!/usr/bin/env python3
"""从固定d23生成四司法命令永久回执的窄接线；中央独占写共享叶。"""
import argparse
import difflib
import hashlib
import json
import subprocess
from pathlib import Path

BASE = 'd23b0bf897ef1103f9159ec6f116b8c608b2c9f5'
MODULE = 'political_shop_licensing_receipts'
HEADER = 'resident/domains/politics/licensing/political_shop_licensing_result_bindings.hpp'


def candidate(repo, output):
    if output.exists():
        raise ValueError('output must be fresh')
    rows, patch = [], []

    def change(path, transform):
        before = subprocess.check_output(['git', '-C', str(repo), 'show', BASE + ':' + path]).decode()
        def one(old, new):
            nonlocal text
            if text.count(old) != 1:
                raise ValueError(f'exact anchor mismatch {path}: {old[:100]!r}')
            text = text.replace(old, new)
        def after_line(token, inserted):
            matching = [line for line in text.splitlines(True) if token in line]
            if len(matching) != 1:
                raise ValueError(f'exact line mismatch {path}: {token}')
            one(matching[0], matching[0] + inserted + '\n')
        text = before
        transform(one, after_line)
        target = output / 'candidate' / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)
        oid = lambda s: subprocess.check_output(['git', 'hash-object', '--stdin'], input=s.encode()).decode().strip()
        rows.append({'path': path, 'before_oid': oid(before), 'after_oid': oid(text), 'sha256': hashlib.sha256(text.encode()).hexdigest()})
        patch.extend(difflib.unified_diff(before.splitlines(True), text.splitlines(True), fromfile='a/' + path, tofile='b/' + path))

    def catalog(one, line):
        one('struct PoliticalShopLicensingState;', 'struct PoliticalShopLicensingState;struct PoliticalShopLicensingReceiptState;')
        line('std::shared_ptr<const PoliticalShopLicensingCatalog> political_shop_licensing;', '    bool political_shop_licensing_receipts=false;')
    change('native/world/include/hanmo/resident/model_contract.hpp', catalog)
    change('native/world/src/resident/model/common_model.hpp', lambda one, line: line('std::shared_ptr<const PoliticalShopLicensingState> political_shop_licensing;', '    std::shared_ptr<const PoliticalShopLicensingReceiptState> political_shop_licensing_receipts;'))

    def bundle_header(one, line):
        one('#pragma once', '#pragma once\n#include "' + HEADER + '"')
        line('PoliticalShopLicensingState political_shop_licensing;', '    std::optional<PoliticalShopLicensingReceiptState> political_shop_licensing_receipts;')
        one('const std::vector<StableId>& appointment_sources={});', 'const std::vector<StableId>& appointment_sources={},const StableId& licensing_source_id={});')
    change('native/world/src/resident/kernel/world_bundle.hpp', bundle_header)

    def bundle(one, line):
        one('political_shop_licensing_seed_complete(bool(seed.political_shop_licensing)),', 'political_shop_licensing_seed_complete(bool(seed.political_shop_licensing)),political_shop_licensing_receipts(seed.political_shop_licensing_receipts?std::optional<PoliticalShopLicensingReceiptState>{*seed.political_shop_licensing_receipts}:std::nullopt),')
        one('political_shop_licensing_seed_complete(other.political_shop_licensing_seed_complete),', 'political_shop_licensing_seed_complete(other.political_shop_licensing_seed_complete),political_shop_licensing_receipts(other.political_shop_licensing_receipts),')
    change('native/world/src/resident/kernel/world_bundle.cpp', bundle)

    def resident(one, line):
        one('auto appointment_sources=personal_appointment_new_result_world(', 'StableId licensing_source_id;if(candidate->catalog->political_shop_licensing_receipts&&political_shop_licensing_command_kind(committed_kind)){auto source=political_shop_licensing_new_player_source(current.political_shop_licensing,candidate->political_shop_licensing,command.actor_id,committed_kind,candidate->common.tick());if(!source.ok)return {false,{},source.error};licensing_source_id=std::move(source.value);}auto appointment_sources=personal_appointment_new_result_world(')
        one('registry_acceptance_id,appointment_sources.value);', 'registry_acceptance_id,appointment_sources.value,licensing_source_id);')
        one('const std::vector<StableId>& appointment_sources){', 'const std::vector<StableId>& appointment_sources,const StableId& licensing_source_id){')
        line('auto appointment_bound=bind_personal_appointment_result_world(', '    // 仅该次新司法业务在真实result分配后绑定；NPC无玩家提交尾，不写此独立表。\n    if(!licensing_source_id.empty()||(w.catalog->political_shop_licensing_receipts&&political_shop_licensing_command_kind(kind))){if(!w.catalog->political_shop_licensing_receipts||!w.political_shop_licensing_receipts||licensing_source_id.empty()||!political_shop_licensing_command_kind(kind))return common_failure("PoliticalShopLicensingNewPlayerSourceRequired");auto bound=bind_political_shop_licensing_player_result(w.common,w.political_shop_licensing,*w.political_shop_licensing_receipts,licensing_source_id,actor,sequence,*receipt,[](const CommitResult& r){return r.details&&std::holds_alternative<Done>(r.details->data);});if(!bound.ok)return bound;}')
    change('native/world/src/resident/kernel/resident_world.cpp', resident)

    def codec(one, line):
        one('#include "resident/domains/politics/licensing/political_shop_registry_profile.hpp"', '#include "resident/domains/politics/licensing/political_shop_registry_profile.hpp"\n#include "' + HEADER + '"')
        one('if(catalog.personal_recovery)domains.push_back({"personal_injury_recovery",1});return domains;', 'if(catalog.personal_recovery)domains.push_back({"personal_injury_recovery",1});if(catalog.political_shop_licensing_receipts)domains.push_back({"political_shop_licensing_receipts",1});return domains;')
        one('{"political_shop_registry_successions",{"1"}}', '{"political_shop_registry_successions",{"1"}}, {"political_shop_licensing_receipts",{"1"}}')
        line('if(has("personal_injury_recovery"))', '        if(has("political_shop_licensing_receipts")){if(!dynamic||!has("political_shop_licensing")||!has("private_shops"))fail("PoliticalShopLicensingReceiptsDependenciesRequired","checkpoint/module_versions");domains.push_back({"political_shop_licensing_receipts",1});}')
        line('if(result->director&&result->director->shop_registry_intents){', '        auto legal_receipts=decode_political_shop_licensing_receipt_profile(scenario,caps);if(!legal_receipts.ok)return {false,{},legal_receipts.error};result->political_shop_licensing_receipts=legal_receipts.value;')
        line('if(result->personal_recovery)id.module_versions', '    if(result->political_shop_licensing_receipts)id.module_versions.emplace_back("political_shop_licensing_receipts","1");')
        line('if(catalog.political_shop_registry_successions){auto parsed=decode_political_shop_registry_successions', '    if(catalog.political_shop_licensing_receipts){auto parsed=decode_political_shop_licensing_receipts(field(source,"political_shop_licensing_receipts","state"));if(!parsed.ok)return {false,{},parsed.error};result.political_shop_licensing_receipts=std::make_shared<const PoliticalShopLicensingReceiptState>(std::move(parsed.value));}else if(source.find("political_shop_licensing_receipts"))fail("UnexpectedPoliticalShopLicensingReceipts","political_shop_licensing_receipts");')
        line('if(w.political_shop_registry_successions_enabled())set(output,', '    if(w.catalog->political_shop_licensing_receipts){if(!w.political_shop_licensing_receipts)fail("PoliticalShopLicensingReceiptsProfileMismatch","state");set(output,"political_shop_licensing_receipts",encode_political_shop_licensing_receipts(*w.political_shop_licensing_receipts));}')
        line('if(w.personal_recovery_enabled()){r=section(', '    if(w.catalog->political_shop_licensing_receipts){if(!w.political_shop_licensing_receipts)return {false,{},common_failure("PoliticalShopLicensingReceiptsProfileMismatch").error};r=section("political_shop_licensing_receipts",[&](auto& c){return visit_political_shop_licensing_receipts_checkpoint(c,*w.political_shop_licensing_receipts);});if(!r.ok)return {false,{},r.error};}')
        line('else if(section.domain=="personal_injury_recovery")', '        else if(section.domain=="political_shop_licensing_receipts"){auto state=std::make_shared<PoliticalShopLicensingReceiptState>();r=visit_political_shop_licensing_receipts_checkpoint(reader,*state);seed.political_shop_licensing_receipts=std::move(state);}')
    change('native/world/src/resident/kernel/world_codec.cpp', codec)

    def initial(one, line):
        one('#include "resident/domains/politics/licensing/political_shop_registry_model.hpp"', '#include "resident/domains/politics/licensing/political_shop_registry_model.hpp"\n#include "' + HEADER + '"')
        line('if(const auto* dynamic=scenario.find("dynamic_world"))if(dynamic->find("personal_injury_recovery"))', '    auto legal_receipts=decode_political_shop_licensing_receipt_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!legal_receipts.ok)return {false,{},legal_receipts.error};if(legal_receipts.value)admitted.module_versions.emplace_back("political_shop_licensing_receipts","1");')
    change('native/world/src/resident/kernel/native_initial_state.cpp', initial)
    change('game/ui/native_resident_backend.gd', lambda one, line: line('if definition.get("dynamic_world",{}).has("personal_injury_recovery"):', '\tif definition.get("dynamic_world",{}).has("political_shop_licensing_receipts"): expected["module_versions"].append({"name":"political_shop_licensing_receipts","version":"1"})'))

    # 共享声明schema/capability在原描述中只添加新显式选择，不改旧法律作者值。
    for path in ['schemas/content.schema.json', 'game/data/rule_capabilities.json', 'tools/content/links.py']:
        before = subprocess.check_output(['git', '-C', str(repo), 'show', BASE + ':' + path]).decode()
        if path.endswith('content.schema.json'):
            from content.political_shop_licensing_result_links import with_political_shop_licensing_receipt_schema
            after = json.dumps(with_political_shop_licensing_receipt_schema(json.loads(before)), ensure_ascii=False, indent=2) + '\n'
        elif path.endswith('rule_capabilities.json'):
            value = json.loads(before)
            value['capabilities'].append({'capability_id': 'political_shop_licensing_receipts_v1', 'rule_version': 'political_shop_licensing_receipts_v1', 'module_id': 'native_dynamic_profile', 'support_status': 'implemented/unverified', 'schema_selectors': [{'path': 'scenarios.*.dynamic_world.political_shop_licensing_receipts.version', 'equals': 1}], 'supported_semantics': [], 'test_ids': ['test_political_shop_licensing_panel'], 'evidence': []})
            after = json.dumps(value, ensure_ascii=False, indent=2) + '\n'
        else:
            after = before.replace('    from .political_shop_licensing_links import check_political_shop_licensing\n', '    from .political_shop_licensing_links import check_political_shop_licensing\n    from .political_shop_licensing_result_links import check_political_shop_licensing_receipts\n')
            if after == before or after.count('check_political_shop_licensing(pack,index)') != 1:
                raise ValueError('exact author link anchor')
            after = after.replace('check_political_shop_licensing(pack,index)', 'check_political_shop_licensing(pack,index) + check_political_shop_licensing_receipts(pack,index)')
        change(path, lambda one, line, before=before, after=after: one(before, after))
    (output / 'candidate.patch').write_text(''.join(patch))
    (output / 'manifest.json').write_text(json.dumps({'base': BASE, 'status': 'SOURCE_ONLY', 'shared': rows, 'cmake': 'central sole writer registers 3 domain CPP, 1 header, 1 native test and author test; no CMake denial', 'compatibility': 'law1 old fields unchanged; new receipt tail last after d23 recovery; no new Common feature or NPC writer'}, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    candidate(args.repo, args.output)

"""固定共享基准的完整提交来源候选；三producer由中央另接，不写正在使用的工作树。"""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.content.private_shop_command_results_links import with_private_shop_command_results_schema

BASE = '13e4570cd4692faf1bcbe8fdafcc813170f480f3'
PATHS = [
    'native/world/include/hanmo/resident/model_contract.hpp',
    'native/world/src/resident/model/common_model.hpp',
    'native/world/src/resident/kernel/world_bundle.hpp',
    'native/world/src/resident/kernel/world_bundle.cpp',
    'native/world/src/resident/kernel/resident_world.cpp',
    'native/world/src/resident/kernel/dynamic_integration.cpp',
    'native/world/src/resident/kernel/phase_plan.cpp',
    'native/world/resident.cmake',
    'schemas/content.schema.json',
    'game/data/rule_capabilities.json',
    'tools/content/links.py',
]
DOMAIN = 'src/resident/domains/commerce/command_results/'
CPP = [DOMAIN + name + '.cpp' for name in ('private_shop_command_result_sources',
    'private_shop_command_results_codec', 'private_shop_command_results_commit',
    'private_shop_command_results_validation', 'private_shop_command_result_player')]
HPP = [DOMAIN + name + '.hpp' for name in ('private_shop_command_results',
    'private_shop_command_result_sources', 'private_shop_command_results_codec',
    'private_shop_command_result_player')]
CPP += ['src/resident/kernel/private_shop_command_results_integration.cpp']
HPP += ['src/resident/kernel/private_shop_command_results_integration.hpp']


def once(text, old, new):
    """原锚点必须唯一，拒绝错基准或重复来源尾。"""
    if text.count(old) != 1:
        raise ValueError(f'Expected one source anchor, got {text.count(old)}: {old[:100]}')
    return text.replace(old, new, 1)


def transform(path, text):
    """只增加独立来源状态与最终完整门；旧Shop字段、访客和早结构读保持。"""
    if path.endswith('model_contract.hpp'):
        text = once(text, 'struct PrivateShopDailyRentCatalog;', 'struct PrivateShopCommandResultsCatalog;struct PrivateShopCommandResultsState;struct PrivateShopDailyRentCatalog;')
        return once(text, '    std::shared_ptr<const PrivateShopDailyRentCatalog> private_shop_daily_rent;', '    std::shared_ptr<const PrivateShopDailyRentCatalog> private_shop_daily_rent;\n    std::shared_ptr<const PrivateShopCommandResultsCatalog> private_shop_command_results;')
    if path.endswith('common_model.hpp'):
        return once(text, '    std::shared_ptr<const PrivateShopDailyRentState> private_shop_daily_rent;', '    std::shared_ptr<const PrivateShopDailyRentState> private_shop_daily_rent;\n    std::shared_ptr<const PrivateShopCommandResultsState> private_shop_command_results;')
    if path.endswith('world_bundle.hpp'):
        text = once(text, '#include "private_shop_daily_rent_integration.hpp"', '#include "private_shop_daily_rent_integration.hpp"\n#include "private_shop_command_results_integration.hpp"')
        text = once(text, '    PrivateShopDailyRentState private_shop_daily_rent;bool private_shop_daily_rent_seed_complete=false;', '    PrivateShopDailyRentState private_shop_daily_rent;bool private_shop_daily_rent_seed_complete=false;\n    PrivateShopCommandResultsState private_shop_command_results;bool private_shop_command_results_seed_complete=false;\n    bool private_shop_command_results_enabled()const{return bool(catalog->private_shop_command_results);}')
        return once(text, 'const StableId& political_succession_source_id={});', 'const StableId& political_succession_source_id={},const WorldBundle* shop_command_before=nullptr);')
    if path.endswith('world_bundle.cpp'):
        text = once(text, 'private_shop_daily_rent_seed_complete(bool(seed.private_shop_daily_rent)),', 'private_shop_daily_rent_seed_complete(bool(seed.private_shop_daily_rent)),private_shop_command_results(seed.private_shop_command_results?*seed.private_shop_command_results:PrivateShopCommandResultsState{}),private_shop_command_results_seed_complete(bool(seed.private_shop_command_results)),')
        text = once(text, 'private_shop_daily_rent_seed_complete(other.private_shop_daily_rent_seed_complete),', 'private_shop_daily_rent_seed_complete(other.private_shop_daily_rent_seed_complete),private_shop_command_results(other.private_shop_command_results),private_shop_command_results_seed_complete(other.private_shop_command_results_seed_complete),')
        # 完整Shop/Task/清算已验，完整Family各控制链之后再核submitter；不反借早Originals。
        anchor = '    valid=validate_private_shop_daily_rent_player_history_world(*this);if(!valid.ok)return valid;'
        return once(text, anchor, anchor + '\n    valid=validate_private_shop_command_results_world(*this);if(!valid.ok)return valid;\n    valid=validate_private_shop_command_result_player_world(*this);if(!valid.ok)return valid;')
    if path.endswith('resident_world.cpp'):
        text = once(text, 'initialized=initialize_private_shop_liquidation_world_in_candidate(*candidate);if(!initialized.ok)return initialized;', 'initialized=initialize_private_shop_liquidation_world_in_candidate(*candidate);if(!initialized.ok)return initialized;initialized=initialize_private_shop_command_results_world_in_candidate(*candidate);if(!initialized.ok)return initialized;')
        text = once(text, 'nullptr,political_succession_source_id);if(!completed.ok)', 'nullptr,political_succession_source_id,&current);if(!completed.ok)')
        text = once(text, 'const StableId& political_succession_source_id){', 'const StableId& political_succession_source_id,const WorldBundle* shop_command_before){')
        anchor = '    if(plan_execution||!player_plan_command_kind(kind)){auto plan=finish_player_plan_commit(w,actor,plan_execution,details,id.value);if(!plan.ok)return plan;}'
        return once(text, anchor, '    // 全部真实Shop写者与实际Common结果已完成，先绑定后让Plan消费；before只活到本次返回。\n    auto shop_bound=bind_private_shop_command_results_world(shop_command_before,w,actor,sequence,*receipt);if(!shop_bound.ok)return shop_bound;\n' + anchor)
    if path.endswith('dynamic_integration.cpp'):
        text = once(text, '    PrivateShopDailyRentState private_shop_daily_rent;', '    PrivateShopDailyRentState private_shop_daily_rent;\n    PrivateShopCommandResultsState private_shop_command_results;')
        text = once(text, 'private_shop_daily_rent(w.private_shop_daily_rent),', 'private_shop_daily_rent(w.private_shop_daily_rent),private_shop_command_results(w.private_shop_command_results),')
        return once(text, 'w.private_shop_daily_rent=std::move(private_shop_daily_rent);', 'w.private_shop_daily_rent=std::move(private_shop_daily_rent);w.private_shop_command_results=std::move(private_shop_command_results);')
    if path.endswith('phase_plan.cpp'):
        anchor = '    auto daily_rent=shop_command&&w.private_shop_daily_rent_enabled()?std::optional<PrivateShopDailyRentState>{w.private_shop_daily_rent}:std::nullopt;'
        text = once(text, anchor, anchor + '\n    auto shop_results=w.private_shop_command_results_enabled()?std::optional<PrivateShopCommandResultsState>{w.private_shop_command_results}:std::nullopt;')
        return once(text, 'if(daily_rent)w.private_shop_daily_rent=std::move(*daily_rent);', 'if(daily_rent)w.private_shop_daily_rent=std::move(*daily_rent);if(shop_results)w.private_shop_command_results=std::move(*shop_results);')
    if path.endswith('resident.cmake'):
        text = once(text, 'add_library(hanmo_resident STATIC\n', 'add_library(hanmo_resident STATIC\n' + ''.join('    '+p+'\n' for p in CPP))
        text = once(text, 'set(HANMO_RESIDENT_IDENTITY_FILES\n', 'set(HANMO_RESIDENT_IDENTITY_FILES\n' + ''.join('    '+p+'\n' for p in CPP+HPP))
        return text + '\n# 实际提交来源/裁剪边界用例，必须另在匹配正式库执行。\nadd_executable(test_resident_private_shop_command_results tests/resident/economic_world/private_shop_command_results_test.cpp)\ntarget_link_libraries(test_resident_private_shop_command_results PRIVATE hanmo_resident)\ntarget_include_directories(test_resident_private_shop_command_results PRIVATE src)\ntarget_compile_options(test_resident_private_shop_command_results PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\nadd_test(NAME resident_private_shop_command_results COMMAND test_resident_private_shop_command_results)\n'
    if path.endswith('content.schema.json'):
        return json.dumps(with_private_shop_command_results_schema(json.loads(text)), ensure_ascii=False, indent=2)+'\n'
    if path.endswith('rule_capabilities.json'):
        value = json.loads(text)
        if any(row['capability_id']=='private_shop_command_results_v1' for row in value['capabilities']):
            raise ValueError('PrivateShopCommandResultsAlreadyRegistered')
        value['capabilities'].append({'capability_id':'private_shop_command_results_v1', 'rule_version':'actual_shop_submission_results_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.private_shop_command_results.version','equals':1}],'supported_semantics':[],'test_ids':['test_private_shop_panel_guard'],'evidence':[]})
        return json.dumps(value,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('tools/content/links.py'):
        text = once(text, '    from .private_shop_daily_rent_links import check_private_shop_daily_rent', '    from .private_shop_daily_rent_links import check_private_shop_daily_rent\n    from .private_shop_command_results_links import check_private_shop_command_results')
        return once(text, 'check_private_shop_daily_rent(pack,index) +', 'check_private_shop_daily_rent(pack,index) + check_private_shop_command_results(pack,index) +')
    raise ValueError(path)


def main():
    """读取固定Git原叶与新鲜输出；所有后像、原OID和外部待配套显式列入manifest。"""
    parser=argparse.ArgumentParser();parser.add_argument('--repo',required=True);parser.add_argument('--base',default=BASE);parser.add_argument('--output',required=True);args=parser.parse_args()
    if args.base!=BASE:raise ValueError('FixedShopResultBaseRequired')
    out=Path(args.output);out.mkdir(parents=True,exist_ok=False);rows=[];patch=[]
    for path in PATHS:
        before=subprocess.check_output(['git','-C',args.repo,'show',args.base+':'+path],text=True);after=transform(path,before);target=out/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(after)
        blob=lambda s:hashlib.sha1(b'blob '+str(len(s.encode())).encode()+b'\0'+s.encode()).hexdigest()
        rows.append({'path':path,'base_oid':blob(before),'after_oid':blob(after)});patch.extend(difflib.unified_diff(before.splitlines(True),after.splitlines(True),'a/'+path,'b/'+path))
    (out/'candidate.patch').write_text(''.join(patch));(out/'manifest.json').write_text(json.dumps({'base':BASE,'status':'SHARED_CANDIDATE_EXTERNAL_THREE_PRODUCERS_PENDING','shared':rows,'external_shared':['native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/native_initial_state.cpp','game/ui/native_resident_backend.gd'],'remaining':['independent module/section all three producers','same-generation syntax and behavioral risk checks']},ensure_ascii=False,indent=2)+'\n');print(out/'manifest.json')


if __name__=='__main__':main()

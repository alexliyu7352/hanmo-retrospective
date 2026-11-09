"""用已发布作者索引核最小卡片作者片段，不制造伪造初态或宣称完整打包成功。"""
import argparse
from copy import deepcopy
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('growth_cards', ROOT / 'tools/content/growth_cards.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def main():
    """固定源OID，验证作者真实本领、启用人物与初态来源，坏输入不改原数据。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--catalog', type=Path, default=ROOT / 'docs/native/shared-world-foundation-20261008/author-qualified/foundation-author-qualified.hmcatalog.json')
    args = parser.parse_args()
    raw = args.catalog.read_bytes()
    digest = hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()
    fragment = json.loads((ROOT / 'docs/native/system-character-life/cards/author-slice.json').read_text())
    assert digest == fragment['base_catalog_git_oid'], 'actual author source OID mismatch'
    index = json.loads(raw)['catalog']
    original = deepcopy(index)
    working = deepcopy(index)
    working['texts'].update(fragment['texts'])
    scenario = deepcopy(index['scenarios'][fragment['scenario_id']])
    scenario['growth_cards'] = fragment['growth_cards']
    pack = {'cards': fragment['cards'], 'scenarios': [scenario]}
    checks = 0

    def check(value, reason):
        """精确标明作者行为，失败时不以空诊断充当通过。"""
        nonlocal checks
        checks += 1
        assert value, reason

    check(not MODULE.check_growth_cards(pack, working), 'real two-card source slice must pass')
    bad = deepcopy(pack)
    bad['cards'][0]['prerequisites'] = {'kind': 'always'}
    check(any(row[0] == 'CARD_PREREQUISITE' for row in MODULE.check_growth_cards(bad, working)), 'cannot bypass actual ability qualification')
    bad = deepcopy(pack)
    bad['cards'][0]['acquire_condition'] = {'kind': 'open_page'}
    check(any(row[0] == 'CARD_ACQUISITION' for row in MODULE.check_growth_cards(bad, working)), 'viewing cannot be an acquisition source')
    bad = deepcopy(pack)
    bad['cards'][0]['category'] = 'title'
    check(any(row[0] == 'CARD_EFFECT_UNSUPPORTED' for row in MODULE.check_growth_cards(bad, working)), 'unfinished category must be explicit')
    bad = deepcopy(pack)
    bad['cards'][1]['effect_params']['personal_skill_id'] = 'missing'
    check(any(row[0] == 'CARD_ABILITY' for row in MODULE.check_growth_cards(bad, working)), 'card cannot point to fabricated ability')
    bad = deepcopy(pack)
    bad['scenarios'][0]['growth_cards']['initial_grants'] = []
    check(any(row[0] == 'CARD_INITIAL_SOURCE_REQUIRED' for row in MODULE.check_growth_cards(bad, working)), 'real initial mastered abilities need explicit source')
    bad = deepcopy(pack)
    bad['scenarios'][0]['growth_cards']['initial_grants'][0]['actor_id'] = 'society::actor.apprentice'
    check(any(row[0] == 'CARD_INITIAL_ABILITY' for row in MODULE.check_growth_cards(bad, working)), 'grant cannot teach unlearned ability to apprentice')
    legacy = deepcopy(pack)
    legacy['scenarios'][0].pop('growth_cards')
    check(not MODULE.check_growth_cards(legacy, working), 'passive definitions do not silently enable new profile')
    check(index == original and args.catalog.read_bytes() == raw, 'author source index remains unchanged')
    print(f'GROWTH_CARDS_AUTHOR_PY {checks} checks 0 failures; base {digest}')


if __name__ == '__main__':
    main()

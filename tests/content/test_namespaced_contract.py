"""复用真实Schema引用验证限定ID与历史Schema归档，不模拟游戏规则。"""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import pytest
from tools.content.validation import SCHEMA, TABLES, validate_pack
from tools.content.references import iter_references
from tools import content_stage

ROOT = Path(__file__).resolve().parents[2]
NAMESPACE = 'n' * 32


def qualified_pack():
    """只在测试副本按真实引用注解限定全局ID，保留对白局部顺序和节点ID。"""
    pack = json.loads((ROOT / 'content/foundation/procurement.json').read_text())
    references = list(iter_references(pack))
    for path, _, value in references:
        parent = pack
        for part in path[:-1]: parent = parent[part]
        parent[path[-1]] = NAMESPACE + '::' + value
    for table in TABLES:
        for row in pack.get(table, []): row['id'] = NAMESPACE + '::' + row['id']
    for table in ('texts', 'flags'):
        pack[table] = {NAMESPACE + '::' + key: value for key, value in pack[table].items()}
    return pack


def test_qualified_global_ids_keep_real_reference_checks():
    """当前80字符拒绝会使本例失败；放宽后仍须检出不存在的限定场所引用。"""
    pack = qualified_pack()
    assert validate_pack(pack).ok
    refs = list(iter_references(pack))
    assert any(table == 'places' and '::' in value for _, table, value in refs)
    pack['actors'][0]['initial_place'] = NAMESPACE + '::missing.place'
    assert any(issue.code == 'REFERENCE' for issue in validate_pack(pack).issues)


def test_maximum_actor_id_and_local_node_boundary():
    """114字符人物ID可引用；局部对白节点不因静态ID放宽而允许命名空间。"""
    pack = qualified_pack()
    old = pack['actors'][0]['id']; new = NAMESPACE + '::' + 'a' * 80
    for path, table, value in list(iter_references(pack)):
        if table != 'actors' or value != old: continue
        parent = pack
        for part in path[:-1]: parent = parent[part]
        parent[path[-1]] = new
    pack['actors'][0]['id'] = new
    assert len(new) == 114 and validate_pack(pack).ok
    bad = deepcopy(pack); bad['actors'][0]['id'] += 'a'
    assert not validate_pack(bad).ok
    bad = deepcopy(pack); bad['dialogues'][0]['nodes'][0]['id'] = 'foreign::node'
    assert not validate_pack(bad).ok


def test_ref_definitions_own_annotations_without_siblings():
    """真实walkers遇ref立即返回，目标定义必须自己携带引用和编辑器注解。"""
    root = SCHEMA['$defs']['actor']['properties']['initial_place']
    assert '$ref' in root and set(root) == {'$ref'}
    target = SCHEMA['$defs'][root['$ref'].split('/')[-1]]
    assert target['x-hanmo-ref'] == 'places' and target['x-editor'] == 'reference'
    assert '$ref' not in target


def test_schema_archive_index_binds_exact_bytes(tmp_path):
    """索引只选当前或受信归档原字节，文件内容与摘要名不一致必须拒绝。"""
    assert hasattr(content_stage, 'schema_sources')
    root = tmp_path / 'source'; (root / 'schemas/content-history').mkdir(parents=True)
    current = b'{"type":"object"}\n'; old = b'{"type":"string"}\n'
    (root / 'schemas/content.schema.json').write_bytes(current)
    digest = hashlib.sha256(old).hexdigest()
    archive = root / 'schemas/content-history' / (digest + '.json'); archive.write_bytes(old)
    sources = content_stage.schema_sources(root)
    assert set(sources) == {digest, hashlib.sha256(current).hexdigest()}
    target = tmp_path / 'generated'; target.mkdir()
    content_stage.write_schema_index(root, target)
    index = json.loads((target / 'schema-index.json').read_text())
    assert index['schemas'][digest] == 'schemas/' + digest + '.json'
    assert (target / index['schemas'][digest]).read_bytes() == old
    archive.write_bytes(current)
    with pytest.raises(ValueError, match='digest|摘要'):
        content_stage.schema_sources(root)


@pytest.mark.parametrize('value', ['base::actor\n', 'n' * 32 + '::' + 'a' * 80 + '\n', 'base::actor\r', 'base::actor\r\n'])
def test_qualified_id_requires_absolute_string_end(value):
    """JSON Schema的美元终锚可停在末尾换行前，跨语言静态ID必须拒绝整串尾控制符。"""
    from tools.content.validation import StrictValidator
    rule = {'$defs': SCHEMA['$defs'], **SCHEMA['$defs']['actor']['properties']['id']}
    assert not StrictValidator(rule).is_valid(value)

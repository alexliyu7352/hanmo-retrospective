"""校验带版本的作者内容及引用；缺少可选表按空表查询，不修改作者源或世界。"""
from dataclasses import dataclass, asdict
import json
from pathlib import Path
from typing import Any, List
from jsonschema import Draft202012Validator, validators
from .links import check_links, required_capabilities
from .graphs import check_graphs
from .growth_cards import check_growth_cards
from .actor_cards import check_actor_cards
from .adult_adoption import check_adult_adoption
from .voluntary_succession import check_voluntary_succession
from .family_voluntary_paths import check_family_voluntary_paths
from .family_life import check_family_life
from .family_legacy import check_family_legacy
from .family_biological_legacy import check_family_biological_legacy
from .family_collateral_legacy import check_family_collateral_legacy
from .family_bequest import check_family_bequest
from .family_inheritance import check_family_inheritance
from .family_marriage import check_family_marriage
from .family_apprenticeship import check_family_apprenticeship
from .family_disciple_legacy import check_family_disciple_legacy
from .family_birth import check_family_birth
from .child_care_relief import check_child_care_relief
from .personal_recovery import check_personal_recovery
from .tactic_lessons import check_tactic_lessons
from .family_innate_learning import check_family_innate_learning
from .teaching_requests import check_teaching_requests
from .study_teaching_requests import check_study_teaching_requests
from .household_control import check_household_control
from .character_creation import check_character_creation
from .household_provision_management_links import check_household_provision_management
from .production_disruption_links import check_production_disruptions
from .insight_cards import check_insight_cards
from .capability_contract import load_contract, check_supported_semantics

ROOT = Path(__file__).resolve().parents[2]
SCHEMA_PATH = ROOT / 'schemas/content.schema.json'
SCHEMA_BYTES = SCHEMA_PATH.read_bytes()
SCHEMA = json.loads(SCHEMA_BYTES.decode('utf8'))
Draft202012Validator.check_schema(SCHEMA)


def _integer(checker, value):
    """排除布尔值和浮点拼写，保持游戏定义中的整数契约。"""
    return type(value) is int


StrictValidator = validators.extend(
    Draft202012Validator,
    type_checker=Draft202012Validator.TYPE_CHECKER.redefine('integer', _integer),
)
VALIDATOR = StrictValidator(SCHEMA)
TABLES = tuple(SCHEMA['x-hanmo-tables'])


@dataclass(frozen=True)
class Issue:
    """保留稳定错误码和源字段位置，供编辑器导航到问题。"""
    code: str
    path: str
    message: str
    record_id: str = ''


@dataclass
class ValidationReport:
    """汇总静态问题；报告无错误也不能证明运行时或平台可用。"""
    issues: List[Issue]

    @property
    def ok(self) -> bool:
        """判断已经执行的静态检查是否全部通过，不推断未运行的行为。"""
        return not self.issues

    def to_dict(self) -> dict:
        """序列化静态报告元数据，保留尚未进行运行验证的明确标记。"""
        return {'status': 'STRUCTURE_VALID' if self.ok else 'INVALID_CONTENT',
                'runtime_verified': False, 'issues': [asdict(i) for i in self.issues]}

    @classmethod
    def from_schema(cls, errors, source):
        """先把结构错误转换成可定位诊断，再允许执行跨记录语义索引。"""
        return cls(sorted([Issue('SCHEMA', source + '#' + pointer(e.absolute_path),
                                 e.message[:600]) for e in errors],
                          key=lambda i: (i.path, i.code, i.message)))


def pointer(parts) -> str:
    """转义JSON指针并保留根地址，供编辑器准确定位字段。"""
    return '/' + '/'.join(str(x).replace('~', '~0').replace('/', '~1') for x in parts)


def _fragment_valid(fragment, value) -> bool:
    """选择Schema分支时仅解析随项目提供的本地定义。"""
    return StrictValidator({'$defs': SCHEMA['$defs'], **fragment}).is_valid(value)


def _references(value, schema, path=()):
    """沿已经通过形状检查的结构枚举稳定ID引用。"""
    if '$ref' in schema:
        yield from _references(value, SCHEMA['$defs'][schema['$ref'].split('/')[-1]], path)
        return
    for keyword in ('oneOf', 'anyOf'):
        if keyword in schema:
            for option in schema[keyword]:
                if _fragment_valid(option, value):
                    yield from _references(value, option, path)
                    break
            return
    if 'x-hanmo-ref' in schema:
        yield path, schema['x-hanmo-ref'], value
    if isinstance(value, dict):
        props = schema.get('properties', {})
        for key, child in value.items():
            child_schema = props.get(key, schema.get('additionalProperties', {}))
            if isinstance(child_schema, dict):
                yield from _references(child, child_schema, path + (key,))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            yield from _references(child, schema.get('items', {}), path + (index,))


def _bounded(value) -> bool:
    """在Schema递归前限制内容深度，并拒绝JSON以外的值类型。"""
    pending = [(value, 0)]
    total = 0
    while pending:
        node, depth = pending.pop()
        total += 1
        if depth > 32 or total > 200000:
            return False
        if isinstance(node, dict):
            if any(not isinstance(k, str) for k in node):
                return False
            pending.extend((v, depth + 1) for v in node.values())
        elif isinstance(node, list):
            pending.extend((v, depth + 1) for v in node)
        elif type(node) not in (str, int, float, bool, type(None)):
            return False
    return True


def validate_pack(pack: Any, source: str = 'content.json') -> ValidationReport:
    """检查结构、引用、权限和拓扑；非法内容提供定位，缺失可选表不自动落盘。"""
    if not _bounded(pack):
        return ValidationReport([Issue('CONTENT_LIMIT', source + '#/', 'Content nesting or size exceeds validation bounds')])
    errors = list(VALIDATOR.iter_errors(pack))
    if errors:
        return ValidationReport.from_schema(errors, source)
    index = {'texts': pack['texts'], 'flags': pack['flags']}
    raw = []
    all_ids = set()
    for table in TABLES:
        index[table] = {}
        for n, record in enumerate(pack.get(table, [])):
            rid = record['id']
            if rid in all_ids:
                raw.append(('DUPLICATE_ID', (table, n, 'id'), 'Duplicate entity ID: ' + rid))
            all_ids.add(rid)
            index[table][rid] = record
    for path, table, target in _references(pack, SCHEMA):
        if target not in index[table]:
            raw.append(('REFERENCE', path, 'Unknown {} reference: {}'.format(table, target)))
    for number, actor in enumerate(pack['actors']):
        if actor.get('origin') == 'historical' and actor.get('appearance_definition', {}).get('kind') == 'generated':
            raw.append(('APPEARANCE_HISTORICAL_FIXED', ('actors', number, 'appearance_definition', 'kind'),
                        '历史人物须声明固定实际外貌，不能按候选随机生成'))
    # 缺失引用会破坏跨记录检查的前提，先报告引用问题再停止后续语义索引。
    if not raw:
        raw.extend(check_supported_semantics(pack, load_contract()))
        raw.extend(check_growth_cards(pack, index))
        raw.extend(check_actor_cards(pack, index))
        raw.extend(check_adult_adoption(pack, index))
        raw.extend(check_voluntary_succession(pack, index))
        raw.extend(check_family_voluntary_paths(pack, index))
        raw.extend(check_family_life(pack, index))
        raw.extend(check_family_legacy(pack, index))
        raw.extend(check_family_biological_legacy(pack, index))
        raw.extend(check_family_collateral_legacy(pack, index))
        raw.extend(check_family_bequest(pack, index))
        raw.extend(check_family_inheritance(pack, index))
        raw.extend(check_family_marriage(pack, index))
        raw.extend(check_family_apprenticeship(pack, index))
        raw.extend(check_family_disciple_legacy(pack, index))
        raw.extend(check_family_birth(pack, index))
        raw.extend(check_child_care_relief(pack, index))
        raw.extend(check_personal_recovery(pack, index))
        raw.extend(check_tactic_lessons(pack, index))
        raw.extend(check_family_innate_learning(pack, index))
        raw.extend(check_teaching_requests(pack, index))
        raw.extend(check_study_teaching_requests(pack, index))
        raw.extend(check_household_control(pack, index))
        raw.extend(check_character_creation(pack, index))
        raw.extend(check_household_provision_management(pack, index))
        raw.extend(check_production_disruptions(pack, index))
        raw.extend(check_insight_cards(pack, index))
        raw.extend(_scenario_semantics(pack, index))
        missing = required_capabilities(pack) - set(pack['required_capabilities'])
        if missing:
            raw.append(('CAPABILITY_DECLARATION', ('required_capabilities',),
                        'Missing declarations: ' + ', '.join(sorted(missing))))
    result = []
    for code, path, message in raw:
        rid = ''
        if len(path) >= 2 and path[0] in TABLES and isinstance(path[1], int):
            rid = pack[path[0]][path[1]].get('id', '')
        result.append(Issue(code, source + '#' + pointer(path), message, rid))
    return ValidationReport(sorted(result, key=lambda i: (i.path, i.code, i.message)))


def _scenario_semantics(pack, index):
    """新剧本沿同一有效视图做活动语义，错误位置映回原作者记录；旧目录保留原检查路径。"""
    from .scenarios import SNAPSHOT_FIELDS, register_schema, resolve
    if not any(any(field in row for field in SNAPSHOT_FIELDS) for row in pack['scenarios']):
        return check_links(pack, index) + check_graphs(pack, index)
    registered = register_schema(SCHEMA_BYTES)
    catalog = {'ok': True, 'data': {**index, 'required_capabilities': pack['required_capabilities']},
               'schema_sha256': registered['schema_sha256']}
    positions = {table: {row['id']: number for number, row in enumerate(pack.get(table, []))}
                 for table in TABLES}
    issues = []
    for number, scenario in enumerate(pack['scenarios']):
        result = resolve(catalog, scenario['id'])
        if not result['ok']:
            suffix = result.get('path', '').split('/')
            tail = tuple(suffix[3:]) if len(suffix) > 2 and suffix[1] == 'scenarios' else ()
            issues.append((result['code'], ('scenarios', number) + tail, result['code']))
            continue
        data = result['catalog']['data']
        view = {**pack, **{table: list(data[table].values()) for table in TABLES},
                'texts': data['texts'], 'flags': data['flags']}
        # 原型结构与引用已在上游校验；活动语义不能借其旧地点激活另一地图。
        view['actors'] = [row for row in view['actors'] if row['id'] in data['active_static_actor_ids']]
        view['scenarios'] = [data['scenarios'][scenario['id']]]
        for actor in view['actors']:
            if actor.get('origin') == 'historical' and actor.get('appearance_definition', {}).get('kind') == 'generated':
                rows = scenario.get('initial', {}).get('actors', [])
                actor_number = next((i for i, row in enumerate(rows) if row['actor_id'] == actor['id']), None)
                path = ('scenarios', number, 'initial', 'actors', actor_number, 'appearance_definition', 'kind') if actor_number is not None else ('actors', positions['actors'][actor['id']], 'appearance_definition', 'kind')
                issues.append(('APPEARANCE_HISTORICAL_FIXED', path, '历史人物初态须声明固定实际外貌，不能按候选随机生成'))
        for code, path, message in check_links(view, data) + check_graphs(view, data):
            if len(path) >= 2 and path[0] in TABLES and isinstance(path[1], int):
                record = view[path[0]][path[1]]
                path = (path[0], positions[path[0]][record['id']]) + tuple(path[2:])
                # 新行军初态诊断按真实ID回查原序；不改既有驻军或其他语义错误的定位。
                if (code == 'MILITARY_TRAINING_PROFILE' and len(path) == 7
                        and path[0] == 'scenarios' and path[2:4] == ('initial', 'garrisons')
                        and path[5] == 'units'):
                    camp = record['initial']['garrisons'][path[4]]
                    camp_number = next(i for i, row in enumerate(scenario['initial']['garrisons'])
                                       if row['id'] == camp['id'])
                    source_camp = scenario['initial']['garrisons'][camp_number]
                    unit_id = camp['units'][path[6]]['id']
                    occurrence = sum(row['id'] == unit_id for row in camp['units'][:path[6]])
                    unit_number = [i for i, row in enumerate(source_camp['units'])
                                   if row['id'] == unit_id][occurrence]
                    path = path[:4] + (camp_number, 'units', unit_number)
            issues.append((code, path, message))
    return issues

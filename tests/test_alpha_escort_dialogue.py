"""护送受托与回报使用不同作者成功节点；不以文字暗示尚未发生的任务结算。"""
from pathlib import Path
import json

from tools.content.document import AuthorDocument

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'content/foundation/alpha_life.json'


def sender_nodes(document):
    """从真实作者节点建立只读索引，测试不伪造游戏任务状态或命令结果。"""
    return {node['id']: node for node in document.record('dialogues', 'dialog.sender')['nodes']}


def test_escort_accept_and_report_use_distinct_success_text():
    """若受托再次指向已结算文案，成功出口及其语义检查会直接失败。"""
    doc = AuthorDocument.open(SOURCE)
    nodes = sender_nodes(doc)
    accepted = nodes[nodes['escort.accept']['on_success']]
    reported = nodes[nodes['escort.report']['on_success']]
    assert accepted['id'] != reported['id']
    assert accepted['kind'] == reported['kind'] == 'say'
    assert accepted['speaker'] == reported['speaker'] == '$provider'
    assert accepted['next'] == reported['next'] == 'end'
    texts = doc.snapshot()['texts']
    assert '先与老邱约好同行' in texts[accepted['text_key']]
    assert '清算' not in texts[accepted['text_key']]
    assert '钱物依约清算' in texts[reported['text_key']]
    assert nodes['escort.accept']['command'] == {'kind': 'accept_offer', 'offer_id': 'offer.escort'}
    assert nodes['escort.report']['command'] == {'kind': 'report_offer', 'offer_id': 'offer.escort'}
    assert nodes['escort.accept']['on_failure'] == nodes['escort.report']['on_failure'] == 'escort.denied'


def test_escort_success_branch_survives_normal_author_save_reopen_prepare(tmp_path):
    """改配走同一作者保存、重开和编译校验；实际目录仍区分受托与回报出口。"""
    doc = AuthorDocument.open(SOURCE)
    authored = tmp_path / 'alpha_life.json'
    saved = doc.save(authored)
    assert saved['status'] == 'AUTHOR_SAVED'
    reopened = AuthorDocument.open(authored)
    assert reopened.snapshot() == doc.snapshot() and not reopened.status()['dirty']
    catalog = tmp_path / 'alpha_life.hmcatalog.json'
    prepared = reopened.prepare(catalog)
    assert prepared['status'] == 'AUTHORING_ONLY'
    data = json.loads(catalog.read_text())['catalog']
    nodes = {row['id']: row for row in data['dialogues']['dialog.sender']['nodes']}
    assert nodes['escort.accept']['on_success'] == 'escort.accepted'
    assert nodes['escort.report']['on_success'] == 'escort.done'
    assert data['texts'][nodes['escort.accepted']['text_key']] == doc.snapshot()['texts']['escort.accepted']

"""以新1.1版本显式开启已受领使者/治理续行，保原人物、初态和本人受约入口。"""
import argparse
import json
from pathlib import Path


def emit(path, value):
    """新作者原件不覆既有版本，也不生成运行期Task或批准。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        raise ValueError('refuse existing author: ' + str(path))
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


def build(repo):
    """原青川外交签发与玩家受领保持；v2只续行已真实接受的责任。"""
    root = repo / 'content/packs/authored'
    original = root / 'envoy_reports_1_0'
    target = root / 'envoy_reports_1_1'
    pack = json.loads((original / 'pack.json').read_text())
    content = json.loads((original / 'content.json').read_text())
    for value in (pack, content):
        value['version'] = '1.1.0'
        value['required_capabilities'].append('task_authored_continuation_v2')
    for scenario in content['scenarios']:
        director = scenario['dynamic_world']['director']
        if 'authored_task_continuation' in director:
            raise ValueError('unexpected existing continuation selection')
        director['authored_task_continuation'] = {'version': 2, 'message_addresses': []}
    emit(target / 'pack.json', pack)
    emit(target / 'content.json', content)
    emit(repo / 'content/packs/selections/task-responsibility-1.1.0.json', {
        'format': 'HM-PACK-SELECTION', 'format_version': 1,
        'packs': [{'id': name, 'version': version} for name, version in [
            ('hanmo.local_society', '1.11.0'), ('hanmo.political_economy', '1.2.0'),
            ('hanmo.diplomatic_responses', '1.0.0'), ('hanmo.envoy_reports', '1.1.0')]]})
    (target / 'README.md').write_text('''# 青川已受领责任续行 1.1

正规选择 `content/packs/selections/task-responsibility-1.1.0.json`，场景仍为 `envoyreport::scenario.qingchuan_envoy_report`。新版本显式选择 `authored_task_continuation/2`，原1.0包保留。初态人物、钱粮、岗位、已知来源、外交期限和既有使命条款不变。

城吏仍须在实际会面时按原权源签发，学徒本人受领；本包不增加NPC自动接受使命的策略，不预制Task或批准。若非玩家人物已通过合法入口承担责任，原Director才按其冻结递交/返报地址有界续行；到场仅向当地真实岗位且确为指定Office有权者交谈，最后由原使者业务命令验权。报告沿原返报责任连续性，不套用递交期限。

治理劳动与赈济结算仍由原Action和政治/经济规则推进；续行仅保留准确等待原因，不另发工单或挪用资源。普通信件地址列表为空，不赋予新的远方人物知识。正常作者准入与局部语法不代表NPC自然受约、完整World或GUI已通过；这些运行门按同代产物另验。
''')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', required=True, type=Path)
    build(parser.parse_args().repo)

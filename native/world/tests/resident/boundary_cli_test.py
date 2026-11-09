"""CLI只验工具边界与能力关闭，不把微型输入或缺core的返回算作游戏链通过。"""
import json
import hashlib
import pathlib
import subprocess
import sys
import tempfile


def run(executable, *args):
    """捕获明确退出码和机器报告，意外日志不覆盖失败。"""
    return subprocess.run([executable, *map(str, args)], text=True, capture_output=True, check=False)


def orders(value, path=()):
    """测试生成纯结构顺序证据，不包含任何领域字段规则。"""
    result = []
    if isinstance(value, dict):
        result.append({'path': list(path), 'keys': list(value)})
        for key, child in value.items():
            result.extend(orders(child, path + (key,)))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            result.extend(orders(child, path + (index,)))
    return result


def artifact(root, name, value):
    """为比较器构造有真SHA的小型工具fixture，不能标为实际游戏证据。"""
    raw = json.dumps(value, separators=(',', ':'))
    order_raw = json.dumps(orders(value), separators=(',', ':'))
    (root / (name + '.json')).write_text(raw)
    (root / (name + '-order.json')).write_text(order_raw)
    return {'file': name + '.json', 'sha256': hashlib.sha256(raw.encode()).hexdigest(),
            'order_file': name + '-order.json', 'order_sha256': hashlib.sha256(order_raw.encode()).hexdigest()}


def probe_comparator(executable, root):
    """完整链比较器必须抓住命令、任意全态类型差异和断链，不只看末值。"""
    catalog = artifact(root, 'catalog', {})
    before = artifact(root, 'before', {'x': 1, 'y': True})
    after = artifact(root, 'after', {'x': 2, 'y': True})
    command = {'kind': 'protocol-fixture', 'request_seq': 1}
    preview, receipt = {'ok': True}, {'ok': True, 'revision': 1}
    index = {'format': 'HM-RESIDENT-JOURNEY-1000-ORACLE-INDEX/1', 'expanded_catalog': catalog,
             'initial_state': before, 'final_state': after,
             'commands': [{'command': command, 'preview': preview, 'receipt': receipt,
                           'before': before, 'after': after}]}
    index_path = root / 'index.json'
    index_path.write_text(json.dumps(index))
    report = {'format': 'HM-RESIDENT-BRIDGE-PROBE/1', 'ok': True,
              'oracle_index_sha256': hashlib.sha256(index_path.read_bytes()).hexdigest(),
              'initial': before['sha256'], 'final': after['sha256'],
              'states': {row['sha256']: row for row in (before, after)},
              'commands': [{'command_json': json.dumps(command), 'preview': preview, 'receipt': receipt,
                            'before': before['sha256'], 'after': after['sha256']}]}
    report_path = root / 'report.json'
    report_path.write_text(json.dumps(report))
    same = run(executable, '--compare-bridge', index_path, report_path)
    assert same.returncode == 0, same.stderr
    assert json.loads(same.stdout)['state_boundaries_compared'] == 4
    prepared = root / 'prepared'
    assert run(executable, '--prepare-bridge', index_path, prepared).returncode == 0
    assert json.loads((prepared / 'command-0.json').read_text()) == command
    assert not (prepared / 'before.json').exists()
    typed = artifact(root, 'after-typed', {'x': 2.0, 'y': True})
    report['states'][typed['sha256']] = typed
    report['final'] = typed['sha256']
    report['commands'][0]['after'] = typed['sha256']
    report_path.write_text(json.dumps(report))
    changed = run(executable, '--compare-bridge', index_path, report_path)
    assert changed.returncode == 1 and not json.loads(changed.stdout)['full_chain_equivalent']
    report['commands'][0]['before'] = typed['sha256']
    report_path.write_text(json.dumps(report))
    assert run(executable, '--compare-bridge', index_path, report_path).returncode == 2


def main():
    """值/类型/字典序三种输入独立验证，且无core的重放必须明确非零。"""
    executable = sys.argv[1]
    with tempfile.TemporaryDirectory(prefix='hanmo-boundary-cli-') as directory:
        root = pathlib.Path(directory)
        probe_comparator(executable, root)
        files = [root / name for name in ('left.json', 'same.json', 'typed.json', 'order.json')]
        texts = ['{"z":1,"a":2}', '{"z":1,"a":2}', '{"z":1.0,"a":2}', '{"a":2,"z":1}']
        for path, text in zip(files, texts):
            path.write_text(text)
        same = run(executable, '--compare', files[0], files[1])
        assert same.returncode == 0 and json.loads(same.stdout)['exact']
        typed = run(executable, '--compare', files[0], files[2])
        assert typed.returncode == 1 and not json.loads(typed.stdout)['values_equal']
        order = run(executable, '--compare', files[0], files[3])
        assert order.returncode == 1
        result = json.loads(order.stdout)
        assert result['values_equal'] and not result['dictionary_order_equal']
        bad = root / 'duplicate.json'
        bad.write_text('{"x":1,"x":2}')
        assert run(executable, '--compare', bad, bad).returncode == 2
        missing = run(executable, '--replay', root / 'missing-index.json', root / 'output')
        assert missing.returncode == 2
        if missing.stdout:
            assert json.loads(missing.stdout)['full_chain_complete'] is False
    print('resident CLI boundary checks passed; no gameplay acceptance')


if __name__ == '__main__':
    main()

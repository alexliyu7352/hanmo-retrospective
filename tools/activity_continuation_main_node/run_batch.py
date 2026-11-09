"""冻结后的唯一Main联合批次；先收集三个真实生产者，再运行各自独立物理消费者，保留全部红证据。"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys
import time


def write(path, value):
    """批次回执保存在新独占目录，任何失败都不覆盖之前的节点结果。"""
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


def main():
    """每阶段只执行一次；失败不修复、不重试、不跳到另一个库，继续独立案例。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binding', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--rendered-producers', action='store_true')
    args = parser.parse_args()
    binding_path = args.binding.resolve(strict=True)
    binding = json.loads(binding_path.read_text())
    if binding.get('native_artifact_status') != 'BOUND_REVIEWED_ARTIFACT_NOT_EXECUTED':
        raise SystemExit('Exact reviewed frontend and successful native build binding required.')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    runner = Path(binding['drivers']['run_node.py']['path'])
    if hashlib.sha256(runner.read_bytes()).hexdigest() != binding['drivers']['run_node.py']['sha256']:
        raise SystemExit('The bound official node launcher changed.')
    result = {'source_commit': binding['source_commit'], 'native_source_commit': binding['native_source_commit'],
              'helper_commit': binding['helper_commit'], 'binding_sha256': hashlib.sha256(binding_path.read_bytes()).hexdigest(),
              'started_unix': time.time(), 'stages': [], 'ok': False}
    producers = {}

    def run(case, phase):
        """调用同一绑定启动器，在真实终态之后写回每阶段结果，绝不把零退出替代原节点回执。"""
        command = [sys.executable, str(runner), '--binding', str(binding_path), '--case-root', str(output / case), '--case', case, '--phase', phase]
        if phase == 'produce' and args.rendered_producers:
            command.append('--rendered')
        started = time.time()
        print(json.dumps({'case': case, 'phase': phase, 'state': 'running'}), flush=True)
        reply = subprocess.run(command, text=True, capture_output=True)
        name = case + '-' + phase
        (output / (name + '.stdout.log')).write_text(reply.stdout)
        (output / (name + '.stderr.log')).write_text(reply.stderr)
        phase_output = output / case / ('producer' if phase == 'produce' else phase)
        process_path = phase_output / 'process.json'
        process = json.loads(process_path.read_text()) if process_path.is_file() else {}
        item = {'case': case, 'phase': phase, 'command': command, 'exit_code': reply.returncode,
                'started_unix': started, 'finished_unix': time.time(), 'output': str(phase_output),
                'ok': reply.returncode == 0 and process.get('ok') is True}
        result['stages'].append(item)
        write(output / 'batch-progress.json', result)
        print(json.dumps({'case': case, 'phase': phase, 'ok': item['ok'], 'output': str(phase_output)}), flush=True)
        return item['ok']

    for case in ['travel', 'rest', 'study']:
        producers[case] = run(case, 'produce')
    for case in ['travel', 'rest', 'study']:
        for phase in ['consume-auto', 'consume-manual']:
            if producers[case]:
                run(case, phase)
            else:
                result['stages'].append({'case': case, 'phase': phase, 'skipped': True, 'reason': 'producer_failed_no_valid_original_bank'})
    result['finished_unix'] = time.time()
    result['ok'] = len(result['stages']) == 9 and all(item.get('ok') is True for item in result['stages'])
    write(output / 'batch-result.json', result)
    print(json.dumps({'ok': result['ok'], 'result': str(output / 'batch-result.json'), 'source_commit': result['source_commit'], 'native_source_commit': result['native_source_commit']}), flush=True)
    raise SystemExit(0 if result['ok'] else 1)


if __name__ == '__main__':
    main()

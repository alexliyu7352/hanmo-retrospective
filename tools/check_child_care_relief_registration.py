"""检查照护冷尾必须独立于拜帖，避免纯字符串先后门遗漏实际条件嵌套。"""
from pathlib import Path
import argparse,json,re


def _masked(source):
    """屏蔽注释与字符串中的括号但保留位置；只用于本项目普通C++声明源。"""
    pattern=r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    return re.sub(pattern,lambda m:' ' * len(m.group()),source)


def selected_tail_is_independent(source):
    """返回真实标题选择中两个if是否同层；care的前置只能由自身profile合同决定。"""
    start=source.index('Outcome<std::vector<CheckpointDomain>> selected_checkpoint_domains(')
    end=source.index('Outcome<CheckpointEnvelope> inspect_known_checkpoint(',start)
    body=source[start:end];mask=_masked(body)
    visit=body.index('if(has("visit_messages"))')
    care=body.index('if(has("family_care_relief"))')
    depth=lambda at:mask[:at].count('{')-mask[:at].count('}')
    return depth(visit)==depth(care)


def main():
    """核真实后像并自证旧嵌套样本会失败；此源门不声称执行二进制冷档。"""
    parser=argparse.ArgumentParser();parser.add_argument('root',type=Path);args=parser.parse_args()
    sample='Outcome<std::vector<CheckpointDomain>> selected_checkpoint_domains(){ if(has("visit_messages")){ v(); } if(has("family_care_relief")){ c(); } } Outcome<CheckpointEnvelope> inspect_known_checkpoint('
    old=sample.replace('v(); } if','v(); if').replace('c(); } }','c(); } } }')
    assert selected_tail_is_independent(sample)
    assert not selected_tail_is_independent(old)
    source=(args.root/'native/world/src/resident/kernel/world_codec.cpp').read_text()
    ok=selected_tail_is_independent(source)
    print(json.dumps({'scope':'SOURCE_ONLY exact if nesting, not runtime','care_independent_of_visit':ok,'regression_samples':2}))
    return 0 if ok else 1

if __name__=='__main__':raise SystemExit(main())

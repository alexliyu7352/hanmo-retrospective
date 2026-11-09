#!/usr/bin/env python3
"""准备明确选定的本地字体与完整许可；只复制到工程构建目录，不安装字体或修改系统。"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct

ROOT = Path(__file__).resolve().parents[1]
EXTENSIONS = {'.otf', '.ttf', '.ttc'}


def prepare(source: Path, game: Path = ROOT/'game', license_path: Path | None = None, face_index: int = 0) -> Path:
    """检查真实字体签名并逐字保留提供的许可，生成可核对摘要；字形覆盖仍由 Godot 和实机检查。"""
    source = source.expanduser().resolve()
    if not source.is_file() or source.suffix.lower() not in EXTENSIONS:
        raise ValueError('请选择有权随游戏分发的本地 .otf、.ttf 或 .ttc 字体。')
    with source.open('rb') as stream:
        signature = stream.read(4)
    if signature not in {b'OTTO',b'\x00\x01\x00\x00',b'true',b'ttcf'} or source.stat().st_size < 12:
        raise ValueError('文件没有可识别的字体签名。')
    if license_path is None or not license_path.is_file() or not license_path.read_bytes().strip():
        raise ValueError('必须提供实际字体版权与分发许可文件；工具不会推断授权。')
    face_count = struct.unpack('>I',source.read_bytes()[8:12])[0] if signature == b'ttcf' else 1
    if type(face_index) is not int or not 0 <= face_index < face_count:
        raise ValueError('字体面索引超出实际字体集合范围。')
    destination = game/'assets'/'fonts'/('cjk'+source.suffix.lower())
    destination.parent.mkdir(parents=True,exist_ok=True)
    if source != destination.resolve():
        shutil.copy2(source,destination)
    # 单一候选避免旧字体优先；仅移除本工具生成的同名构建候选，不触碰来源字体。
    for extension in EXTENSIONS - {destination.suffix}:
        other = destination.with_suffix(extension)
        if other.exists(): other.unlink()
    resource = destination.parent/'cjk.tres'
    resource.write_text('[gd_resource type="FontVariation" load_steps=2 format=3]\n\n'
        '[ext_resource type="FontFile" path="res://assets/fonts/'+destination.name+'" id="1"]\n\n'
        '[resource]\nbase_font = ExtResource("1")\nvariation_face_index = '+str(face_index)+'\n',encoding='utf-8')
    prepared = game.parent/'build/font'
    prepared.mkdir(parents=True, exist_ok=True)
    license_bytes = license_path.read_bytes()
    (prepared/'FONT-LICENSE.txt').write_bytes(license_bytes)
    manifest = {'font_file':destination.name, 'source_name':source.name,
                'sha256':hashlib.sha256(destination.read_bytes()).hexdigest(),
                'face_index':face_index,'resource_sha256':hashlib.sha256(resource.read_bytes()).hexdigest(),
                'license_sha256':hashlib.sha256(license_bytes).hexdigest()}
    (prepared/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return destination


def prepared_font(game: Path = ROOT/'game') -> dict:
    """核对准备记录、唯一字体和完整许可字节；丢失或被替换的依赖不得进入 Web 发布。"""
    prepared = game.parent/'build/font'
    try:
        manifest = json.loads((prepared/'manifest.json').read_text(encoding='utf-8'))
        candidates = [game/'assets/fonts'/('cjk'+extension) for extension in EXTENSIONS]
        candidates = [path for path in candidates if path.is_file()]
        if len(candidates) != 1 or candidates[0].name != manifest['font_file']:
            raise ValueError('字体候选与准备记录不一致。')
        font, license_file = candidates[0], prepared/'FONT-LICENSE.txt'
        if hashlib.sha256(font.read_bytes()).hexdigest() != manifest['sha256']:
            raise ValueError('字体在准备后已改变。')
        if hashlib.sha256((font.parent/'cjk.tres').read_bytes()).hexdigest() != manifest['resource_sha256']:
            raise ValueError('字体面选择在准备后已改变。')
        if not license_file.read_bytes().strip() or hashlib.sha256(license_file.read_bytes()).hexdigest() != manifest['license_sha256']:
            raise ValueError('字体许可在准备后已改变。')
        return {**manifest, 'path':font, 'license_path':license_file}
    except (OSError, KeyError, TypeError, ValueError) as exc:
        raise ValueError('请用 prepare_font.py --font 字体路径 --license 许可路径准备 CJK font：'+str(exc)) from exc


def main() -> None:
    """只处理显式传入的本地依赖；不下载字体，不把文件存在当成许可审查或字形验收。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--font',required=True,type=Path)
    parser.add_argument('--license',required=True,type=Path,dest='license_path')
    parser.add_argument('--face-index',type=int,default=0,help='集合字体的实际面索引，普通单字体使用 0')
    args = parser.parse_args()
    try:
        destination = prepare(args.font,license_path=args.license_path,face_index=args.face_index)
    except (OSError,ValueError) as exc:
        raise SystemExit(str(exc)) from exc
    print(f'已准备本地字体：{destination}\n完整许可与摘要保存在 build/font；导出后仍须核验实际中文字形。')


if __name__ == '__main__':
    main()

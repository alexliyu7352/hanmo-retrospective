#!/usr/bin/env bash
# 只在固定官方Debian容器安装官方包；不改宿主或旧Ubuntu/Godot流水线。
set -euo pipefail
source /etc/os-release
[[ "$ID" == debian && "$VERSION_CODENAME" == trixie && "$(uname -m)" == x86_64 ]]
[[ "${HANMO_GCC_VERSION:?}" == 14.2.0-19 ]]
[[ "${HANMO_TOOLCHAIN_IMAGE:?}" =~ ^debian:trixie@sha256:[0-9a-f]{64}$ ]]
export DEBIAN_FRONTEND=noninteractive PYTHONDONTWRITEBYTECODE=1
export GIT_OPTIONAL_LOCKS=0 GIT_NO_LAZY_FETCH=1 LC_ALL=C.UTF-8 LANG=C.UTF-8
export XDG_CACHE_HOME=/cache/runtime/cache XDG_DATA_HOME=/cache/runtime/data XDG_CONFIG_HOME=/cache/runtime/config
# 仅使用映像自带的Debian archive信任根。首次CA包仍经APT签名及摘要核验。
source_list=/tmp/native-only-debian.list
apt_options=(-o "Dir::Etc::sourcelist=$source_list" -o 'Dir::Etc::sourceparts=-' -o 'Acquire::Retries=3')
printf '%s\n' 'deb [arch=amd64 check-valid-until=no signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] http://snapshot.debian.org/archive/debian/20261027T000000Z/ trixie main' > "$source_list"
apt-get "${apt_options[@]}" update -qq
apt-get "${apt_options[@]}" install -y --no-install-recommends ca-certificates
printf '%s\n' 'deb [arch=amd64 check-valid-until=no signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://snapshot.debian.org/archive/debian/20261027T000000Z/ trixie main' > "$source_list"
apt-get "${apt_options[@]}" update -qq
apt-get "${apt_options[@]}" install -y --no-install-recommends curl git python3 binutils make
# 安装前逐一核APT已验签的包身份，与锁中原始包摘要比较；不只相信版本字符串。
python3 - <<'PYPACKAGES'
import json,pathlib,subprocess,sys
sys.path.insert(0,'/src/tools/ci/native_only')
import verify
lock=json.loads(pathlib.Path('/src/native/dependencies.lock.json').read_text())['toolchain']['gcc']
for name,version in lock['packages'].items():
 text=subprocess.check_output(['apt-cache','-o','Dir::Etc::sourcelist=/tmp/native-only-debian.list',
   '-o','Dir::Etc::sourceparts=-','show',name+'='+version],text=True)
 verify.require_package_metadata(text,name,version,lock['package_files'][name])
PYPACKAGES
# 固定编译器、头文件与共享runtime；缺版本就失败，不启用第三方源或自动换版本。
apt-get "${apt_options[@]}" install -y --no-install-recommends \
  "gcc-14=$HANMO_GCC_VERSION" "g++-14=$HANMO_GCC_VERSION" \
  "gcc-14-base=$HANMO_GCC_VERSION" "libgcc-14-dev=$HANMO_GCC_VERSION" \
  "libstdc++-14-dev=$HANMO_GCC_VERSION" "libstdc++6=$HANMO_GCC_VERSION" "libgcc-s1=$HANMO_GCC_VERSION" \
  "gcc-14-x86-64-linux-gnu=$HANMO_GCC_VERSION" "g++-14-x86-64-linux-gnu=$HANMO_GCC_VERSION" \
  "cpp-14=$HANMO_GCC_VERSION" "cpp-14-x86-64-linux-gnu=$HANMO_GCC_VERSION"
mkdir -p /out /cache
for repo in /src /terrain; do git config --global --add safe.directory "$repo"; done
cat > /tmp/native-only-cxx20.cpp <<'CPP'
#include <span>
#include <bit>
#include <cstdint>
#include <bits/c++config.h>
#if !defined(_GLIBCXX_RELEASE) || _GLIBCXX_RELEASE != 14 || _GLIBCXX_USE_CXX11_ABI != 1
#error This source contract requires libstdc++14 headers and the C++11 ABI.
#endif
// 实编运行当前源码所需的标准库与ABI；不只相信编译器版本名。
int main() {
    int a[2] = {3, 4};
    std::span<int> s(a);
    return s.size() == 2 && std::bit_cast<std::uint32_t>(1.0f) == 0x3f800000u ? 0 : 1;
}
CPP
/usr/bin/g++-14 -std=c++20 /tmp/native-only-cxx20.cpp -o /tmp/native-only-cxx20
/tmp/native-only-cxx20
python3 - <<'PY'
import hashlib,json,os,pathlib,subprocess,sys
sys.path.insert(0,'/src/tools/ci/native_only')
import verify
lock=json.loads(pathlib.Path('/src/native/dependencies.lock.json').read_text())['toolchain']['gcc']
packages={name:subprocess.check_output(['dpkg-query','-W','-f=${Version}',name],text=True) for name in lock['packages']}
record={'os_release':pathlib.Path('/etc/os-release').read_text(),
  'image':os.environ['HANMO_TOOLCHAIN_IMAGE'], 'platform':'linux/amd64',
  'acquisition_method':'official_debian_packages',
  'toolchain_repository':'https://snapshot.debian.org/archive/debian/20261027T000000Z/', 'suite':'trixie',
  'bootstrap_ca_repository':'http://snapshot.debian.org/archive/debian/20261027T000000Z/',
  'archive_keyring':'/usr/share/keyrings/debian-archive-keyring.gpg',
  'package_versions':packages, 'cxx20_span_bit_cast_and_libstdcxx14_probe':'PASS',
  'packages':subprocess.check_output(['dpkg-query','-W','-f=${Package}=${Version}\n'],text=True),
  'tools':{}}
for name in ('gcc-14','g++-14','ar','ld'):
 p=pathlib.Path('/usr/bin')/name
 record['tools'][name]={'path':str(p.resolve()),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),
   'version':subprocess.check_output([str(p),'--version'],text=True)}
verify.require_compiler_record(record,lock)
pathlib.Path('/out/compiler.json').write_text(json.dumps(record,indent=2)+'\n')
PY
python3 /src/tools/ci/native_only/bootstrap.py --root /src --cache /cache
python3 - <<'PYREADY'
import hashlib,json,pathlib
lock=pathlib.Path('/src/native/dependencies.lock.json')
pathlib.Path('/out/locked-dependencies-ready.json').write_text(json.dumps({
    'status':'VERIFIED', 'lock_sha256':hashlib.sha256(lock.read_bytes()).hexdigest()})+'\n')
PYREADY
python3 /src/tools/ci/native_only/run.py --root /src --cache /cache --terrain /terrain --output /out --expected-source "$SOURCE_SHA"

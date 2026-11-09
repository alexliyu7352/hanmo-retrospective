#!/usr/bin/env bash
# workflow头持有的监督器；产品仍运行固定source内原build脚本，超时不改其receipt。
set -euo pipefail
source_root=$1
cache=$2
output=$3
source_sha=$4
identity=$5
[[ "$source_sha" =~ ^[0-9a-f]{40}$ ]]
[[ "${GITHUB_RUN_ID:?}" =~ ^[0-9]+$ && "${GITHUB_RUN_ATTEMPT:?}" =~ ^[0-9]+$ ]]
seconds=${HANMO_NATIVE_SOFT_SECONDS:-5400}
[[ "$seconds" =~ ^[0-9]+$ && "$seconds" -ge 1 && "$seconds" -le 5400 ]]
name="hanmo-native-${GITHUB_RUN_ID}-${GITHUB_RUN_ATTEMPT}"
mkdir -p "$output"
[[ ! -e "$output/container.id" && ! -e "$output/build-supervision.json" ]]
python3 - "$0" "$identity" "$output" <<'PY'
import hashlib,json,pathlib,shutil,sys
script,identity,out=map(pathlib.Path,sys.argv[1:])
record=json.loads(identity.read_text())
if hashlib.sha256(script.read_bytes()).hexdigest()!=record['sha256']:
    raise SystemExit('Frozen supervisor differs')
shutil.copyfile(identity,out/'supervisor-source.json')
shutil.copyfile(script,out/'supervise_native_build.sh')
PY
started=$(date -u +%FT%TZ)
status=125
quiescent=false
cleanup_error=''

# 只停止本次唯一具名容器；必须由Docker确认停止/不存在后，才发可保缓存信号。
quiesce() {
  if docker inspect "$name" > "$output/container-final.json" 2> "$output/container-inspect.log"; then
    [[ -f "$output/container.id" ]] || return 1
    if [[ -f "$output/container.id" ]]; then
      python3 - "$output/container.id" "$output/container-final.json" <<'PY' || return 1
import json,pathlib,sys
expected=pathlib.Path(sys.argv[1]).read_text().strip()
rows=json.loads(pathlib.Path(sys.argv[2]).read_text())
if len(rows)!=1 or rows[0].get('Id')!=expected: raise SystemExit('Named container identity differs')
PY
    fi
    running=$(python3 -c 'import json,sys;print(str(json.load(open(sys.argv[1]))[0]["State"]["Running"]).lower())' "$output/container-final.json") || return 1
    if [[ "$running" = true ]]; then
      docker stop --time 30 "$name" >> "$output/container-stop.log" 2>&1 || docker kill "$name" >> "$output/container-stop.log" 2>&1 || return 1
      docker inspect "$name" > "$output/container-final.json" || return 1
    fi
    python3 - "$output/container-final.json" <<'PY' || return 1
import json,pathlib,sys
rows=json.loads(pathlib.Path(sys.argv[1]).read_text())
if len(rows)!=1 or rows[0]['State'].get('Running') is not False or rows[0]['State'].get('Status') not in ('exited','dead','created'):
    raise SystemExit('Native container is still active or state unknown')
PY
    docker rm "$name" >> "$output/container-stop.log" 2>&1 || return 1
  else
    # inspect的单次失败不是无进程证明；用成功的Docker精确名字查询补证。
    local found
    found=$(docker ps -aq --filter "name=^/${name}$") || return 1
    [[ -z "$found" ]] || return 1
  fi
}

# 单独记外层真实退出与receipt摘要，不把旧脚本的RUNNING改成伪原生终态。
finish() {
  trap - EXIT TERM INT HUP
  original_status=$status
  if quiesce; then quiescent=true; else cleanup_error='NATIVE_CONTAINER_STOP_UNCONFIRMED';status=125;fi
  python3 - "$output" "$source_sha" "$name" "$started" "$seconds" "$status" "$original_status" "$quiescent" "$cleanup_error" <<'PY'
import datetime,hashlib,json,pathlib,sys
out=pathlib.Path(sys.argv[1]);source,name,started,seconds,status,original_status,stopped,error=sys.argv[2:]
receipt=out/'native-build-receipt.json'
record={'format':'HANMO-NATIVE-BUILD-SUPERVISION/1','source_commit':source,'container_name':name,
        'started_at':started,'completed_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'soft_limit_seconds':int(seconds),'outer_exit_code':int(status),'build_exit_code':int(original_status),
        'timed_out':int(original_status)==124,'forced_or_signal_exit':int(original_status)==137,
        'container_quiescent':stopped=='true','cache_save_allowed':stopped=='true','error':error,
        'native_receipt_sha256':hashlib.sha256(receipt.read_bytes()).hexdigest() if receipt.exists() else None,
        'native_receipt_rewritten':False,'supervisor_source':json.loads((out/'supervisor-source.json').read_text())}
(out/'build-supervision.json').write_text(json.dumps(record,indent=2)+'\n')
PY
  printf 'cache_save_allowed=%s\n' "$quiescent" >> "${GITHUB_OUTPUT:?}"
  exit "$status"
}
trap finish EXIT
trap 'status=143; exit "$status"' TERM HUP
trap 'status=130; exit "$status"' INT
set +e
timeout --signal=TERM --kill-after=20s "${seconds}s" docker run \
  --name "$name" --cidfile "$output/container.id" \
  -e SOURCE_SHA="$source_sha" \
  -v "$source_root:/src:ro" -v "$cache:/cache" -v "$output:/out" \
  ubuntu:20.04 bash /src/tools/ci/build_ubuntu2004.sh \
  2>&1 | tee "$output/build.log"
statuses=("${PIPESTATUS[@]}")
status=${statuses[0]}
if [[ "$status" = 0 && "${statuses[1]}" != 0 ]]; then status=${statuses[1]};fi
set -e
exit "$status"

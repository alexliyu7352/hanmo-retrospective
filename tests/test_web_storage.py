"""在真实 Chromium 中验证正式 IndexedDB 适配器；桥接结果与 Godot 配置验证分别记录。"""
from __future__ import annotations
import functools
import http.server
import json
import os
import shutil
import pathlib
import threading
import uuid

import pytest
from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parents[1]


def browser_binary():
    """复用显式指定或系统已有的 Chromium 系浏览器，不在测试中下载浏览器或改日常用户资料。"""
    return os.environ.get("CHROMIUM_BIN") or shutil.which("chromium") or shutil.which("google-chrome")


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    """只为测试工程提供本地 HTTP 资源，避免逐请求日志遮蔽失败信息。"""
    def log_message(self, fmt: str, *args: object) -> None:
        """省略 HTTP 访问日志，保留真实浏览器断言与异常输出。"""


@pytest.fixture(scope="module")
def runtime():
    """为本套测试启动独立回环 HTTP 服务和浏览器，结束后关闭进程与监听端口。"""
    server = http.server.ThreadingHTTPServer(
        ("127.0.0.1", 0), functools.partial(QuietHandler, directory=str(ROOT))
    )
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    with sync_playwright() as pw:
        browser = pw.chromium.launch(executable_path=browser_binary(), headless=True,
            args=["--no-sandbox", "--disable-dev-shm-usage"])
        yield browser, f"http://127.0.0.1:{server.server_port}"
        browser.close()
    server.shutdown()
    thread.join(timeout=3)
    server.server_close()


@pytest.fixture
def page(runtime):
    """为每例创建隔离的浏览器存储上下文，并载入实际生产桥接脚本。"""
    browser, base = runtime
    context = browser.new_context(accept_downloads=True)
    tab = context.new_page()
    tab.goto(base + "/tools/storage_probe.html")
    yield tab
    context.close()


def install_helpers(page):
    """构造仅验证存储协议的封装与桥接调用；载荷不冒充真实游戏世界。"""
    page.evaluate("""() => {
      window.bridgeCall = (operation, args={}) => new Promise(resolve =>
        HMBridge.dispatch(operation,JSON.stringify(args),result=>resolve(JSON.parse(result))));
      window.makeEnvelope = async (generation, marker='alpha') => {
        const payload_json = JSON.stringify({fixture:'storage-only',marker});
        const bytes = new TextEncoder().encode(payload_json);
        const digest = await crypto.subtle.digest('SHA-256', bytes);
        return JSON.stringify({magic:'HM-SAVE',format_version:1,
          ruleset_id:'e0-1',content_version:'e0-1',engine_build:'test-fixture',
          rng_backend:'test-fixture',save_id:'manual-1',generation,
          payload_length:bytes.length,
          payload_sha256:Array.from(new Uint8Array(digest),b=>b.toString(16).padStart(2,'0')).join(''),
          payload_json});
      };
    }""")


def test_adapter_is_available(page):
    """缺失生产适配器时必须首先失败，不能以探针页面加载成功替代实现。"""
    assert page.evaluate("typeof window.HMStorage") == "object"


def test_empty_slot_is_not_a_successful_save(page):
    """从未写入的槽位返回明确空状态，不能伪称已保存。"""
    result = page.evaluate("""async () => {
      const s=await HMStorage.open('empty'); const r=await s.get('manual-1'); s.close(); return r;
    }""")
    assert result == {"ok": False, "code": "NotFound", "generation": 0}


def test_save_readback_and_reopen(page):
    """写入内容须在关闭连接并重开后保持一致，不能依赖内存副本。"""
    install_helpers(page)
    result = page.evaluate("""async () => {
      const text=await makeEnvelope(1); let s=await HMStorage.open('reopen');
      const put=await s.put('manual-1',text,0); s.close();
      s=await HMStorage.open('reopen');const read=await s.get('manual-1');
      const slots=await s.list();s.close(); return {put,read,slots,text};
    }""")
    assert result["put"]["ok"] and result["read"]["ok"]
    assert result["read"]["text"] == result["text"]
    assert result["slots"][0]["generation"] == 1


def test_page_refresh_preserves_data(page):
    """刷新实际页面后从数据库恢复原字节，不以页面变量伪装持久存储。"""
    install_helpers(page)
    expected = page.evaluate("""async () => {
      const s=await HMStorage.open('refresh'); const t=await makeEnvelope(1,'刷新后仍在');
      await s.put('manual-1',t,0);s.close();return t;
    }""")
    page.reload()
    actual = page.evaluate("""async () => {
      const s=await HMStorage.open('refresh'); const r=await s.get('manual-1'); s.close();return r;
    }""")
    assert actual["text"] == expected


def test_old_tab_cannot_overwrite_new_generation(page, runtime):
    """另一标签页持有的旧代数不能覆盖已经完成的新代存档。"""
    install_helpers(page)
    page.evaluate("window.storePromise=HMStorage.open('tabs')")
    other = page.context.new_page()
    other.goto(runtime[1] + "/tools/storage_probe.html")
    install_helpers(other)
    other.evaluate("window.storePromise=HMStorage.open('tabs')")
    first = page.evaluate("""async()=> (await storePromise).put('manual-1',await makeEnvelope(1,'new'),0)""")
    stale = other.evaluate("""async()=> (await storePromise).put('manual-1',await makeEnvelope(1,'old'),0)""")
    assert first["ok"]
    assert stale["code"] == "GenerationConflict" and not stale["ok"]
    read = other.evaluate("async()=> (await storePromise).get('manual-1')")
    assert json.loads(json.loads(read["text"])["payload_json"])["marker"] == "new"
    other.close()


def test_same_generation_concurrent_writes_have_one_winner(page):
    """同代并发写入只能有一个成功，失败方不能突破代数比较。"""
    install_helpers(page)
    result = page.evaluate("""async()=> {
      const a=await HMStorage.open('race'),b=await HMStorage.open('race');
      const t=await makeEnvelope(1);
      const r=await Promise.all([a.put('manual-1',t,0),b.put('manual-1',t,0)]);
      a.close();b.close();return r;
    }""")
    assert sorted(r["ok"] for r in result) == [False, True]
    assert next(r for r in result if not r["ok"])["code"] == "GenerationConflict"


def test_digest_mismatch_does_not_overwrite(page):
    """摘要损坏的输入在触碰旧快照前被拒绝。"""
    install_helpers(page)
    result = page.evaluate("""async()=> {
      const s=await HMStorage.open('digest'); await s.put('manual-1',await makeEnvelope(1),0);
      let t=JSON.parse(await makeEnvelope(2));t.payload_json+=' ';
      const bad=await s.put('manual-1',JSON.stringify(t),1);
      const good=await s.get('manual-1');s.close();return {bad,good};
    }""")
    assert not result["bad"]["ok"]
    assert result["good"]["generation"] == 1


def test_abort_after_request_success_is_failure(page):
    """真实写请求成功后中止事务，仍须报告失败并保留旧代。"""
    install_helpers(page)
    result = page.evaluate("""async()=> {
      const s=await HMStorage.open('abort');await s.put('manual-1',await makeEnvelope(1),0);
      const original=IDBObjectStore.prototype.put;
      IDBObjectStore.prototype.put=function(value,key) {
        const req=key===undefined?original.call(this,value):original.call(this,value,key);
        if(this.name==='snapshots') req.addEventListener('success',()=>this.transaction.abort());
        return req;
      };
      const bad=await s.put('manual-1',await makeEnvelope(2),1);
      IDBObjectStore.prototype.put=original;
      const read=await s.get('manual-1');s.close();return {bad,read};
    }""")
    assert not result["bad"]["ok"] and result["bad"]["code"] == "TransactionAborted"
    assert result["read"]["generation"] == 1


def test_permission_or_quota_failure_has_no_success(page):
    """模拟配额异常不能替换上一份有效存档，也不能返回成功。"""
    install_helpers(page)
    result = page.evaluate("""async()=>{
      const s=await HMStorage.open('quota');await s.put('manual-1',await makeEnvelope(1),0);
      const original=IDBObjectStore.prototype.put;
      IDBObjectStore.prototype.put=function(){throw new DOMException('full','QuotaExceededError')};
      const result=await s.put('manual-1',await makeEnvelope(2),1);
      IDBObjectStore.prototype.put=original;
      const old=await s.get('manual-1');s.close();return {result,old};
    }""")
    assert not result["result"]["ok"]
    assert result["result"]["code"] == "QuotaExceeded"
    assert result["old"]["generation"] == 1


def test_invalid_slot_and_wrong_generation_rejected(page):
    """拒绝路径形状的槽位及与封装不符的代数，防止元数据相互矛盾。"""
    install_helpers(page)
    result = page.evaluate("""async()=> {
      const s=await HMStorage.open('invalid');const t=await makeEnvelope(4);
      const a=await s.put('../bad',t,0),b=await s.put('manual-1',t,0);
      s.close();return [a,b];
    }""")
    assert result[0]["code"] == "InvalidSlot"
    assert result[1]["code"] == "InvalidGeneration"


def test_retains_only_current_and_previous_snapshots(page):
    """三次写入只保留最近两代，避免存档快照无限增长。"""
    install_helpers(page)
    result = page.evaluate("""async()=> {
      const s=await HMStorage.open('retention');
      for(let g=1;g<=3;g++) await s.put('manual-1',await makeEnvelope(g),g-1);
      s.close();
      return await new Promise((resolve,reject)=>{
        const q=indexedDB.open('retention');q.onsuccess=()=>{
          const db=q.result, tx=db.transaction('snapshots','readonly');
          const r=tx.objectStore('snapshots').getAllKeys();
          tx.oncomplete=()=>{db.close();resolve(r.result)};
          tx.onerror=()=>reject(tx.error);
        };
      });
    }""")
    assert len(result) == 2
    assert sorted(result) == ["manual-1:2", "manual-1:3"]


def test_corrupt_latest_recovers_previous_and_preserves_slot_generation(page):
    """恢复旧有效载荷时保留最新比较代数，后续保存不能覆盖并发写入。"""
    install_helpers(page)
    result = page.evaluate("""async()=>{
      const s=await HMStorage.open('recovery');
      await s.put('manual-1',await makeEnvelope(1,'old-valid'),0);
      await s.put('manual-1',await makeEnvelope(2,'bad-new'),1);s.close();
      await new Promise((resolve,reject)=>{
        const r=indexedDB.open('recovery');r.onsuccess=()=>{
          const db=r.result,tx=db.transaction('snapshots','readwrite');
          tx.objectStore('snapshots').put({id:'manual-1:2',text:'broken'});
          tx.oncomplete=()=>{db.close();resolve()};tx.onerror=()=>reject(tx.error);
        };
      });
      const s2=await HMStorage.open('recovery'),read=await s2.get('manual-1');
      const next=await s2.put('manual-1',await makeEnvelope(3,'repaired'),read.generation);
      s2.close();return {read,next};
    }""")
    assert result["read"]["ok"] and result["read"]["recovered"]
    assert result["read"]["snapshot_generation"] == 1
    assert result["read"]["generation"] == 2
    assert result["next"]["ok"] and result["next"]["generation"] == 3


def test_duplicate_json_keys_are_rejected(page):
    """封装中的重复键必须失败，不能被普通 JSON 解析静默覆盖。"""
    install_helpers(page)
    result = page.evaluate("""async()=>{
      const s=await HMStorage.open('dup');const t=await makeEnvelope(1);
      const r=await s.put('manual-1','{"magic":"WRONG",'+t.slice(1),0);s.close();return r;
    }""")
    assert not result["ok"] and result["code"] == "InvalidEnvelope"


def test_unknown_format_is_rejected(page):
    """未知封装版本明确拒绝，不能被显示为兼容存档。"""
    install_helpers(page)
    result = page.evaluate("""async()=>{
      const s=await HMStorage.open('future');let t=JSON.parse(await makeEnvelope(1));t.format_version=999;
      const r=await s.put('manual-1',JSON.stringify(t),0);s.close();return r;
    }""")
    assert result["code"] == "UnsupportedFormat"


def test_upgrade_blocked_is_reported_not_deleted(page):
    """旧连接阻挡升级时报告明确原因，不删除旧数据库来绕过阻挡。"""
    result = page.evaluate("""async()=>{
      const old=await new Promise((resolve,reject)=>{
        const q=indexedDB.open('blocked',1);q.onsuccess=()=>resolve(q.result);q.onerror=()=>reject(q.error);
      });
      let answer;
      try { const newer=await HMStorage.open('blocked');newer.close();answer='unexpected'; }
      catch(e){answer=e.code;}
      old.close();return answer;
    }""")
    assert result == "UpgradeBlocked"


def test_download_exports_exact_envelope(page, tmp_path):
    """用户下载的备份必须保留完整编码字节，不能经有损重建改变内容。"""
    install_helpers(page)
    text = page.evaluate("makeEnvelope(1,'download')")
    page.evaluate("t=>window.exportText=t", text)
    page.evaluate("document.getElementById('probe-export').onclick=()=>HMStorage.download(window.exportText,'backup.hmsave')")
    with page.expect_download() as download:
        page.get_by_role("button", name="导出测试快照").click()
    output = tmp_path / "download.hmsave"
    download.value.save_as(output)
    assert output.read_text("utf-8") == text


def test_file_picker_reads_exact_envelope(page, tmp_path):
    """真实文件选择器返回原始 UTF-8 文本，导入选择本身不执行载荷。"""
    install_helpers(page)
    text = page.evaluate("makeEnvelope(1,'imported')")
    file = tmp_path / "import.hmsave"
    file.write_text(text, encoding="utf-8")
    page.evaluate("document.getElementById('probe-import').onclick=()=>HMStorage.pickFile().then(x=>window.picked=x)")
    with page.expect_file_chooser() as chooser:
        page.get_by_role("button", name="导入测试快照").click()
    chooser.value.set_files(file)
    page.wait_for_function("window.picked !== undefined")
    assert page.evaluate("window.picked.text") == text


def test_default_profile_reuses_legacy_database_without_copying(page):
    """缺省与空 profile 都直接读写 hanmo-e0；显式发行的新库为空时不会误读旧局。"""
    install_helpers(page)
    result = page.evaluate("""async () => {
      const legacy=await HMStorage.open();
      const old=await makeEnvelope(1,'旧默认档仍在');
      await legacy.put('manual-1',old,0);legacy.close();
      const omitted=await bridgeCall('get',{slot:'manual-1'});
      const empty=await bridgeCall('get',{slot:'manual-1',profile:''});
      const isolated=await bridgeCall('get',{slot:'manual-1',profile:'new-release'});
      const next=await makeEnvelope(2,'仍写入旧库');
      const put=await bridgeCall('put',{slot:'manual-1',profile:'',text:next,expected_generation:1});
      const reopened=await HMStorage.open(),read=await reopened.get('manual-1');reopened.close();
      return {old,omitted,empty,isolated,put,read,next};
    }""")
    assert result['omitted']['text'] == result['empty']['text'] == result['old']
    assert result['isolated'] == {'ok': False, 'code': 'NotFound', 'generation': 0}
    assert result['put']['ok'] and result['read']['text'] == result['next']


def test_profiles_are_isolated_during_interleaved_and_concurrent_requests(page):
    """交错及并行请求使用各自连接、槽位和代数，不被一个全局连接缓存串到其他发行。"""
    install_helpers(page)
    result = page.evaluate("""async () => {
      const profiles=['release-a','release-b',''];
      const texts=await Promise.all(profiles.map(p=>makeEnvelope(1,p||'legacy')));
      const puts=await Promise.all(profiles.map((profile,i)=>bridgeCall('put',
        {profile,slot:'manual-1',text:texts[i],expected_generation:0})));
      const a=await bridgeCall('get',{profile:'release-a',slot:'manual-1'});
      const b=await bridgeCall('get',{profile:'release-b',slot:'manual-1'});
      const again=await bridgeCall('get',{profile:'release-a',slot:'manual-1'});
      const legacy=await bridgeCall('get',{slot:'manual-1'});
      const lists=await Promise.all(profiles.map(profile=>bridgeCall('list',{profile})));
      const blank=await bridgeCall('list',{profile:'another-release'});
      return {texts,puts,a,b,again,legacy,lists,blank};
    }""")
    assert all(put['ok'] for put in result['puts'])
    assert result['a']['text'] == result['again']['text'] == result['texts'][0]
    assert result['b']['text'] == result['texts'][1]
    assert result['legacy']['text'] == result['texts'][2]
    assert all(row['ok'] and len(row['slots']) == 1 and row['slots'][0]['generation'] == 1 for row in result['lists'])
    assert result['blank'] == {'ok': True, 'slots': []}


@pytest.mark.parametrize('profile', ['../release-a', 'release/a', ' release-a', 'release_a', 'A', 'a' * 65, None, 42])
def test_invalid_profile_cannot_fall_back_or_alias_another_database(page, profile):
    """非法或被改写后相似的标识必须明确失败，不能清洗成另一发行名或回退到旧库。"""
    install_helpers(page)
    result = page.evaluate("""async profile => {
      const s=await HMStorage.open();const text=await makeEnvelope(1,'不可被非法profile覆盖');
      await s.put('manual-1',text,0);
      const bad=await bridgeCall('put',{profile,slot:'manual-1',text:await makeEnvelope(2,'wrong'),expected_generation:1});
      const read=await s.get('manual-1');s.close();return {bad,read,text};
    }""", profile)
    assert result['bad']['code'] == 'InvalidProfile' and not result['bad']['ok']
    assert result['read']['text'] == result['text']


def test_profile_selection_survives_page_refresh(page):
    """页面刷新后 profile 由请求重新指定，恢复各自字节而非依赖上一个页面的连接变量。"""
    install_helpers(page)
    texts = page.evaluate("""async () => {
      const result={};
      for(const profile of ['refresh-a','refresh-b']) {
        const text=await makeEnvelope(1,profile);
        const put=await bridgeCall('put',{profile,slot:'manual-1',text,expected_generation:0});
        if(!put.ok) throw Error(JSON.stringify(put));result[profile]=text;
      }
      return result;
    }""")
    page.reload()
    install_helpers(page)
    for profile, text in texts.items():
        read = page.evaluate("profile=>bridgeCall('get',{profile,slot:'manual-1'})", profile)
        assert read['text'] == text


def test_profile_import_export_stays_in_selected_database(page, tmp_path):
    """用户真实下载并导入相同字节，只保存到所选发行；导入动作本身不悄悄写槽位。"""
    install_helpers(page)
    text = page.evaluate("""async () => {
      const text=await makeEnvelope(1,'跨发行手动备份');
      await bridgeCall('put',{profile:'export-release',slot:'manual-1',text,expected_generation:0});
      window.backupText=(await bridgeCall('get',{profile:'export-release',slot:'manual-1'})).text;
      document.getElementById('probe-export').onclick=()=>bridgeCall('export',
        {profile:'export-release',text:backupText,filename:'release-backup.hmsave'});
      return text;
    }""")
    with page.expect_download() as download:
        page.get_by_role('button', name='导出测试快照').click()
    path = tmp_path / 'release-backup.hmsave'
    download.value.save_as(path)
    assert path.read_text('utf-8') == text
    page.evaluate("""() => document.getElementById('probe-import').onclick=()=>
      bridgeCall('import',{profile:'import-release'}).then(result=>window.profileImport=result)""")
    with page.expect_file_chooser() as chooser:
        page.get_by_role('button', name='导入测试快照').click()
    chooser.value.set_files(path)
    page.wait_for_function('window.profileImport !== undefined')
    result = page.evaluate("""async () => {
      const imported=window.profileImport;
      const before=await bridgeCall('list',{profile:'import-release'});
      const put=await bridgeCall('put',{profile:'import-release',slot:'manual-1',text:imported.text,expected_generation:0});
      const source=await bridgeCall('get',{profile:'export-release',slot:'manual-1'});
      return {imported,before,put,source};
    }""")
    assert result['imported']['ok'] and result['imported']['text'] == text
    assert result['before']['slots'] == [] and result['put']['ok']
    assert result['source']['generation'] == 1 and result['source']['text'] == text
    page.reload()
    install_helpers(page)
    assert page.evaluate("bridgeCall('get',{profile:'import-release',slot:'manual-1'})")['text'] == text


def test_profiles_persist_after_browser_process_restart(runtime, tmp_path):
    """使用测试专属用户目录彻底关闭并重启浏览器进程，确认旧库与两个发行均从磁盘恢复。"""
    browser, base = runtime
    directory = tmp_path / 'browser-profile'
    texts = {}
    for attempt in range(2):
        context = browser.browser_type.launch_persistent_context(str(directory), executable_path=browser_binary(), headless=True,
            args=['--no-sandbox', '--disable-dev-shm-usage'])
        try:
            tab = context.pages[0]
            tab.goto(base + '/tools/storage_probe.html')
            install_helpers(tab)
            for profile in ['', 'restart-a', 'restart-b']:
                if attempt == 0:
                    text = tab.evaluate('marker=>makeEnvelope(1,marker)', profile or 'legacy')
                    result = tab.evaluate("args=>bridgeCall('put',args)",
                        {'profile': profile, 'slot': 'manual-1', 'text': text, 'expected_generation': 0})
                    assert result['ok']
                    texts[profile] = text
                else:
                    result = tab.evaluate("profile=>bridgeCall('get',{profile,slot:'manual-1'})", profile)
                    assert result['ok'] and result['text'] == texts[profile]
        finally:
            context.close()

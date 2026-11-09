/** Unit tests of the real JavaScript envelope boundary, not IndexedDB durability. */
const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const adapter = path.resolve(__dirname, '../web/storage_bridge.js');
if (fs.existsSync(adapter)) require(adapter);
const api = globalThis.HMStorage || {};

/** Produce an internally consistent opaque snapshot for the storage layer. */
async function envelope(payload = {fixture:'storage-only',note:'行旅'}, generation=1) {
  const payload_json=JSON.stringify(payload),bytes=new TextEncoder().encode(payload_json);
  const digest=await crypto.subtle.digest('SHA-256',bytes);
  return JSON.stringify({magic:'HM-SAVE',format_version:1,ruleset_id:'e0-1',content_version:'e0-1',
    engine_build:'test-fixture',rng_backend:'test-fixture',save_id:'manual-1',generation,
    payload_json,payload_length:bytes.length,
    payload_sha256:Array.from(new Uint8Array(digest),x=>x.toString(16).padStart(2,'0')).join('')});
}

/** Change a header without modifying the checked payload. */
async function changed(field,value) {
  const e=JSON.parse(await envelope());e[field]=value;return JSON.stringify(e);
}

test('published interface exists before callers can use storage',()=>{
  assert.equal(typeof api.inspectEnvelope,'function');
  assert.equal(typeof api.open,'function');
});
test('valid UTF-8 snapshot roundtrips exact text',async()=>{
  const t=await envelope();const r=await api.inspectEnvelope(t);
  assert.equal(r.ok,true);assert.equal(r.envelope.payload_json,JSON.parse(t).payload_json);
});
test('UTF-8 length counts bytes rather than JavaScript characters',async()=>{
  const e=JSON.parse(await envelope());e.payload_length=e.payload_json.length;
  assert.equal((await api.inspectEnvelope(JSON.stringify(e))).code,'PayloadLengthMismatch');
});
test('same-length tamper fails its digest',async()=>{
  const e=JSON.parse(await envelope());e.payload_json=e.payload_json.replace('storage','storagE');
  assert.equal((await api.inspectEnvelope(JSON.stringify(e))).code,'ChecksumMismatch');
});
test('future format is not silently accepted',async()=>{
  assert.equal((await api.inspectEnvelope(await changed('format_version',2))).code,'UnsupportedFormat');
});
test('missing required header is rejected',async()=>{
  const e=JSON.parse(await envelope());delete e.ruleset_id;
  assert.equal((await api.inspectEnvelope(JSON.stringify(e))).ok,false);
});
test('fractional generation rejected',async()=>{
  assert.equal((await api.inspectEnvelope(await changed('generation',1.5))).ok,false);
});
test('negative generation rejected',async()=>{
  assert.equal((await api.inspectEnvelope(await changed('generation',-1))).ok,false);
});
test('wrong magic rejected',async()=>{
  assert.equal((await api.inspectEnvelope(await changed('magic','WRONG'))).ok,false);
});
test('duplicate header keys rejected rather than last-key-wins',async()=>{
  const t=await envelope();assert.equal((await api.inspectEnvelope('{"magic":"X",'+t.slice(1))).code,'InvalidEnvelope');
});
test('duplicate payload keys rejected even with valid checksum',async()=>{
  const e=JSON.parse(await envelope());e.payload_json='{"tick":0,"tick":1}';
  const b=new TextEncoder().encode(e.payload_json);e.payload_length=b.length;
  e.payload_sha256=Buffer.from(await crypto.subtle.digest('SHA-256',b)).toString('hex');
  assert.equal((await api.inspectEnvelope(JSON.stringify(e))).code,'InvalidPayload');
});
test('deep nested payload rejected before game loading',async()=>{
  let obj={a:1};for(let i=0;i<34;i++)obj={a:obj};
  assert.equal((await api.inspectEnvelope(await envelope(obj))).code,'InvalidPayload');
});
test('NaN is not a valid JSON value',async()=>{
  assert.equal((await api.inspectEnvelope('{"value":NaN}')).ok,false);
});
test('oversized text field rejected',async()=>{
  assert.equal((await api.inspectEnvelope(await envelope({note:'x'.repeat(16385)}))).code,'InvalidPayload');
});
test('string escape handling does not confuse duplicate detection',async()=>{
  const r=await api.inspectEnvelope(await envelope({note:'"a":1,\\,[]{}\n漢末'}));assert.equal(r.ok,true);
});
test('slot allowlist accepts bounded autos and manual slots',()=>{
  for (const s of ['auto-1','auto-3','manual-1','manual-10']) assert.equal(api.validSlot(s),true);
  for (const s of ['../x','manual-0','manual-11','auto-4','manual-01','']) assert.equal(api.validSlot(s),false);
});
test('missing IndexedDB produces explicit unavailable error',async()=>{
  await assert.rejects(api.open('unit-test'),e=>e.code==='StorageUnavailable');
});
test('parseStrict checks trailing tokens and duplicate escaped key names',()=>{
  assert.throws(()=>api.parseStrict('{"a":1} garbage'));
  assert.throws(()=>api.parseStrict('{"a":1,"\\u0061":2}'));
});

test('envelope cannot be written to a different slot identity',async()=>{
  const text=await envelope();
  const result=await HMStorage.inspectEnvelope(text,'manual-2');
  assert.equal(result.ok,false);
  assert.equal(result.code,'SaveIdMismatch');
});

test('matching slot identity is accepted by the same real validator',async()=>{
  const text=await envelope();
  assert.equal((await HMStorage.inspectEnvelope(text,'manual-1')).ok,true);
});

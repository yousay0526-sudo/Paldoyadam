/* The production browser backend, with deterministic storage failures. */
const { readFileSync } = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const values = new Map();
let failKey = '', failName = '';
const context = { Module: {}, HEAPU8: new Uint8Array(1024), localStorage: {
  getItem(k) { if (failName === 'SecurityError') throw { name: failName }; return values.get(k) ?? null; },
  setItem(k, v) { if (k === failKey || failName === 'SecurityError') throw { name: failName }; values.set(k, v); }
}};
vm.runInNewContext(readFileSync('web/save-storage.js', 'utf8'), context);
const store = context.Module.paldoSaveStore;
const key = 'paldoyadam.journey.autosave';
values.set('paldoyadam.keys', 'KEEP');
assert.equal(store.read(0, 0, 512), -1);
context.HEAPU8.set([1, 2, 3, 4], 0);
assert.equal(store.write(0, 4, 0), 0);
assert.equal(values.get(key), '01020304');
context.HEAPU8.set([5, 6, 7, 8], 0);
assert.equal(store.write(0, 4, 1), 0);
assert.equal(values.get(key + '.backup'), '01020304');
failKey = key; failName = 'QuotaExceededError';
assert.equal(store.write(0, 4, 1), 5);
assert.equal(values.get(key), '05060708');
failName = 'SecurityError';
assert.equal(store.read(0, 0, 512), -4);
assert.equal(store.write(0, 4, 0), 4);
failName = ''; failKey = '';
values.set(key, 'zz'); assert.equal(store.read(0, 0, 512), -2);
values.set(key, '1'); assert.equal(store.read(0, 0, 512), -2);
values.set(key, '00'.repeat(513)); assert.equal(store.read(0, 0, 512), -2);
values.set(key, 'abcdef'); assert.equal(store.read(0, 10, 512), 3);
assert.deepEqual(Array.from(context.HEAPU8.slice(10, 13)), [171, 205, 239]);
assert.equal(values.get('paldoyadam.keys'), 'KEEP');
console.log('PASS: browser storage atomic replacement, backup, quota, security, bounds, key isolation');

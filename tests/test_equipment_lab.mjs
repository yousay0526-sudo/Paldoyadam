import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync,existsSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {SLOT_LABELS,normalizeSelection,presetSelection,selectionFile,readSelectionFile,partFiles} from '../web/equipment-lab/lab-core.mjs';
const root=new URL('../web/equipment-lab/assets/',import.meta.url);
const catalog=JSON.parse(readFileSync(new URL('catalog.json',root),'utf8'));

test('every independent combination survives a selection-file round trip',()=>{
  let combinations=[{}];
  for(const slot of Object.keys(SLOT_LABELS))combinations=combinations.flatMap(state=>['',...catalog.slots[slot].map(i=>i.id)].map(id=>({...state,[slot]:id})));
  assert.equal(combinations.length,768);
  for(const state of combinations)assert.deepEqual(readSelectionFile(catalog,JSON.parse(JSON.stringify(selectionFile(state)))),state);
  assert.equal(presetSelection(catalog,'base').weapon,'');
  assert.deepEqual(presetSelection(catalog,'general'),normalizeSelection(catalog,catalog.default));
});

test('invalid imports are rejected and old stored IDs fall back safely',()=>{
  const valid=selectionFile(catalog.default);
  for(const input of [null,{}, {...valid,version:2},{...valid,pose:'walk'}, {...valid,selection:{weapon:'unknown'}}])assert.throws(()=>readSelectionFile(catalog,input));
  assert.equal(normalizeSelection(catalog,{upper:'deleted-upper'}).upper,'');
});

test('every referenced PNG has matching dimensions, checksum and canvas bounds',()=>{
  const parts=[catalog.body,...Object.values(catalog.erase),...Object.values(catalog.slots).flatMap(items=>items.flatMap(i=>i.layers))];
  assert.equal(new Set(Object.values(catalog.slots).flatMap(items=>items.map(i=>i.id))).size,14);
  assert.equal(partFiles(catalog).length,parts.length);
  for(const part of parts){
    assert.match(part.file,/^[a-z-]+\.png$/);
    const path=new URL(part.file,root);assert.ok(existsSync(path));
    const bytes=readFileSync(path);assert.equal(bytes.subarray(1,4).toString(),'PNG');
    assert.equal(bytes.readUInt32BE(16),part.width);assert.equal(bytes.readUInt32BE(20),part.height);
    assert.equal(bytes[25],6,'RGBA PNG required');
    assert.equal(createHash('sha256').update(bytes).digest('hex'),part.sha256);
    assert.ok(part.x>=0&&part.y>=0&&part.x+part.width<=1254&&part.y+part.height<=1254);
  }
});

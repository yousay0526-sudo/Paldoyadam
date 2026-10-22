export const SLOT_LABELS = Object.freeze({upper:"상의",lower:"하의",head:"투구 · 머리 장비",belt:"허리띠",weapon:"무기"});

export function normalizeSelection(catalog, input = {}, strict = false) {
  const result = {};
  for (const slot of Object.keys(SLOT_LABELS)) {
    const id = typeof input[slot] === "string" ? input[slot] : "";
    const valid = !id || catalog.slots[slot].some(item => item.id === id);
    if (!valid && strict) throw new Error("알 수 없는 장비가 들어 있습니다: " + slot);
    result[slot] = valid ? id : "";
  }
  return result;
}

export function presetSelection(catalog, name) {
  if (name === "base") return normalizeSelection(catalog);
  return normalizeSelection(catalog, Object.fromEntries(Object.keys(SLOT_LABELS).map(slot => [slot, name + "-" + slot])));
}

export function selectedItems(catalog, state) {
  return catalog.order.map(slot => catalog.slots[slot].find(item => item.id === state[slot])).filter(Boolean);
}

export function selectionFile(state) {
  return {format:"paldoyadam-equipment-selection",version:1,pose:"idle-front-oblique",selection:{...state}};
}

export function readSelectionFile(catalog, input) {
  if (!input || input.format !== "paldoyadam-equipment-selection" || input.version !== 1 ||
      input.pose !== "idle-front-oblique" || !input.selection || typeof input.selection !== "object") {
    throw new Error("이 개발자 모드의 선택 파일이 아닙니다.");
  }
  return normalizeSelection(catalog, input.selection, true);
}

export function partFiles(catalog) {
  return [...new Set([catalog.body, ...Object.values(catalog.erase),
    ...Object.values(catalog.slots).flatMap(items => items.flatMap(item => item.layers))].map(part => part.file))];
}

export function drawSelection(context, catalog, images, state, showBody = true) {
  const [width, height] = catalog.canvas;
  const scratch = context.canvas.ownerDocument.createElement("canvas");
  scratch.width = width; scratch.height = height;
  const base = scratch.getContext("2d");
  const draw = (ctx, part) => ctx.drawImage(images.get(part.file), part.x, part.y, part.width, part.height);
  const items = selectedItems(catalog, state);
  if (showBody) {
    draw(base, catalog.body);
    base.globalCompositeOperation = "destination-out";
    for (const id of new Set(items.flatMap(item => item.erase))) draw(base, catalog.erase[id]);
    base.globalCompositeOperation = "source-over";
  }
  const layers = items.flatMap(item => item.layers).filter(part => part.condition !== "no-weapon" || !state.weapon);
  for (const part of layers.filter(part => part.pass !== "hands")) draw(base, part);
  for (const part of layers.filter(part => part.pass === "hands")) draw(base, part);
  context.clearRect(0, 0, width, height);
  context.drawImage(scratch, 0, 0);
  return items;
}

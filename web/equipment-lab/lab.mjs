import {SLOT_LABELS,normalizeSelection,presetSelection,selectionFile,readSelectionFile,partFiles,drawSelection} from "./lab-core.mjs";
const status=document.querySelector("#status");
const canvas=document.querySelector("#preview");
const storageKey="paldoyadam-equipment-lab-v1";
let catalog,state,images;

function message(text,error=false){status.textContent=text;status.classList.toggle("error",error);}
function persist(){try{localStorage.setItem(storageKey,JSON.stringify(selectionFile(state)));}catch{/* Private browsing may disable storage. */}}
function render(){
  const items=drawSelection(canvas.getContext("2d"),catalog,images,state,document.querySelector("#show-body").checked);
  for(const select of document.querySelectorAll("select[data-slot]"))select.value=state[select.dataset.slot];
  document.querySelector("#layer-names").textContent=items.length?items.map(item=>item.id+"\n  "+item.layers.map(p=>p.file).join("\n  ")).join("\n"):"기본 몸만 표시";
  message(items.length+"개 부위 선택 · "+(document.querySelector("#show-body").checked?"기본 몸 표시":"장비만 표시"));
  persist();
}
function download(blob,name){
  const url=URL.createObjectURL(blob);const link=document.createElement("a");
  link.href=url;link.download=name;document.body.append(link);link.click();link.remove();
  setTimeout(()=>URL.revokeObjectURL(url),1000);
}
try{
  const response=await fetch("./assets/catalog.json");if(!response.ok)throw new Error("장비 목록을 읽지 못했습니다.");
  catalog=await response.json();state=normalizeSelection(catalog,catalog.default);
  try{const saved=localStorage.getItem(storageKey);if(saved)state=readSelectionFile(catalog,JSON.parse(saved));}catch{/* Old or invalid saved data is ignored. */}
  images=new Map();
  await Promise.all(partFiles(catalog).map(file=>new Promise((resolve,reject)=>{
    const image=new Image();image.onload=()=>{images.set(file,image);resolve();};
    image.onerror=()=>reject(new Error("이미지를 읽지 못했습니다: "+file));image.src="./assets/"+file;
  })));
  const slots=document.querySelector("#slots");
  for(const [slot,label] of Object.entries(SLOT_LABELS)){
    const wrapper=document.createElement("div");wrapper.className="slot";
    const title=document.createElement("label");title.htmlFor="slot-"+slot;title.textContent=label;
    const select=document.createElement("select");select.id=title.htmlFor;select.dataset.slot=slot;
    for(const item of [{id:"",name:slot==="head"?"없음 · 기본 머리":"없음"},...catalog.slots[slot]]){
      const option=document.createElement("option");option.value=item.id;option.textContent=item.name;select.append(option);
    }
    select.addEventListener("change",()=>{state=normalizeSelection(catalog,{...state,[slot]:select.value});render();});
    wrapper.append(title,select);slots.append(wrapper);
  }
  document.querySelector("#pose").textContent=catalog.pose;
  document.querySelectorAll("[disabled]").forEach(element=>element.disabled=false);
  for(const button of document.querySelectorAll("[data-preset]"))button.addEventListener("click",()=>{state=presetSelection(catalog,button.dataset.preset);document.querySelector("#show-body").checked=true;render();});
  document.querySelector("#show-body").addEventListener("change",render);
  document.querySelector("#randomize").addEventListener("click",()=>{
    state=Object.fromEntries(Object.keys(SLOT_LABELS).map(slot=>{const choices=["",...catalog.slots[slot].map(item=>item.id)];return[slot,choices[Math.floor(Math.random()*choices.length)]];}));render();
  });
  document.querySelector("#save-png").addEventListener("click",()=>canvas.toBlob(blob=>{if(blob)download(blob,"paldoyadam-equipment.png");else message("PNG 저장에 실패했습니다.",true);},"image/png"));
  document.querySelector("#save-json").addEventListener("click",()=>download(new Blob([JSON.stringify(selectionFile(state),null,2)+"\n"],{type:"application/json"}),"paldoyadam-equipment-selection.json"));
  document.querySelector("#load-json").addEventListener("change",async event=>{
    const file=event.target.files[0];if(!file)return;
    try{if(file.size>32768)throw new Error("선택 파일이 너무 큽니다.");state=readSelectionFile(catalog,JSON.parse(await file.text()));render();message("조합을 불러왔습니다.");}
    catch(error){message(error.message,true);}finally{event.target.value="";}
  });
  render();
}catch(error){message(error.message+" 로컬 서버로 열었는지도 확인해 주세요.",true);}
for(const button of document.querySelectorAll("[data-bg]"))button.addEventListener("click",()=>{
  document.querySelector(".stage").dataset.background=button.dataset.bg;
  document.querySelectorAll("[data-bg]").forEach(other=>other.setAttribute("aria-pressed",String(other===button)));
});

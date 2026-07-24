#pragma once

#include <pgmspace.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ED060KD1 WiFi Uploader</title>
<style>
:root{color-scheme:light;--ink:#1d1d1f;--muted:#6e6e73;--line:rgba(0,0,0,.12);--soft:#f5f5f7;--panel:rgba(255,255,255,.88);--action:#0071e3;--accent:#34c759;--warn:#ff3b30;--shadow:0 18px 46px rgba(0,0,0,.08)}
*{box-sizing:border-box}
body{margin:0;font-family:-apple-system,BlinkMacSystemFont,Segoe UI,system-ui,Arial,sans-serif;background:var(--soft);color:var(--ink);-webkit-font-smoothing:antialiased}
header{height:66px;display:flex;align-items:center;justify-content:space-between;gap:16px;padding:0 24px;background:rgba(255,255,255,.72);border-bottom:1px solid var(--line);backdrop-filter:saturate(180%) blur(18px);position:sticky;top:0;z-index:2}
h1{margin:0;font-size:20px;font-weight:700;letter-spacing:0}
main{display:grid;grid-template-columns:minmax(320px,390px) minmax(0,1fr);gap:18px;padding:18px;min-height:calc(100vh - 66px)}
.panel{background:var(--panel);border:1px solid rgba(255,255,255,.72);border-radius:22px;padding:16px;box-shadow:var(--shadow);backdrop-filter:saturate(180%) blur(18px);min-width:0}
.stack{display:flex;flex-direction:column;gap:12px}
.section{border-top:1px solid var(--line);padding-top:14px;min-width:0}
.section:first-child{border-top:0;padding-top:0}
.title{font-size:13px;font-weight:750;margin:0 0 10px;color:#2b2b2f}
label{display:block;font-size:12px;font-weight:650;color:var(--muted);margin-bottom:6px}
input,select,button{font:inherit}
input[type=file],input[type=text],input[type=password],input[type=number],select{width:100%;height:38px;border:1px solid var(--line);border-radius:12px;background:rgba(255,255,255,.94);padding:0 11px;color:var(--ink);outline:none;min-width:0}
input:focus,select:focus{border-color:rgba(0,113,227,.55);box-shadow:0 0 0 4px rgba(0,113,227,.12)}
input[type=range]{width:100%}
input[type=checkbox]{accent-color:var(--action)}
button{height:38px;border:1px solid var(--action);background:var(--action);color:#fff;border-radius:999px;padding:0 14px;cursor:pointer;display:inline-flex;align-items:center;justify-content:center;gap:6px;white-space:nowrap;min-width:0;overflow:hidden;text-overflow:ellipsis;font-weight:650}
button.secondary{background:rgba(255,255,255,.92);color:var(--action)}
button.danger{background:rgba(255,255,255,.92);color:var(--warn);border-color:rgba(255,59,48,.7)}
button:disabled{opacity:.55;cursor:not-allowed}
.icon-btn{width:42px;min-width:42px;padding:0;font-size:20px;line-height:1}
.row{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:9px;min-width:0}
.row3{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr) minmax(0,1fr);gap:9px;min-width:0}
.row4{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:9px;min-width:0}
.rotate-row{grid-template-columns:42px 42px minmax(0,1fr)}
.inline{display:flex;align-items:center;gap:8px}
.inline input[type=checkbox]{width:auto;height:auto}
.status{font-size:12px;line-height:1.55;color:var(--muted);background:rgba(255,255,255,.7);border:1px solid var(--line);border-radius:14px;padding:10px 12px;min-height:44px;white-space:pre-wrap;overflow-wrap:anywhere}
.preview-wrap{height:calc(100vh - 102px);display:flex;align-items:center;justify-content:center;background:#fff;border:1px solid rgba(255,255,255,.8);border-radius:22px;overflow:hidden;box-shadow:var(--shadow);min-width:0}
canvas{max-width:100%;max-height:100%;background:#fff;box-shadow:0 4px 24px rgba(0,0,0,.10);touch-action:none}
footer{color:var(--soft);background:var(--soft);font-size:11px;text-align:center;padding:0 0 12px;text-shadow:none}
.slots{display:grid;grid-template-columns:repeat(auto-fit,minmax(112px,1fr));gap:8px;min-width:0}
.slot{position:relative;border:1px solid var(--line);border-radius:14px;padding:9px;background:rgba(255,255,255,.72);font-size:12px;min-width:0;overflow:hidden;cursor:pointer;transition:transform .16s ease,background .16s ease,border-color .16s ease,box-shadow .16s ease}
.slot:hover{transform:translateY(-1px)}
.slot.filled{border-color:rgba(52,199,89,.45);background:rgba(52,199,89,.08)}
.slot.selected{animation:bluePulse 1.05s ease-in-out infinite;border-color:#0071e3;background:rgba(0,113,227,.10)}
.slot.current{animation:greenPulse 1.15s ease-in-out infinite;border-color:#34c759;background:rgba(52,199,89,.12)}
.slot-head{display:flex;align-items:flex-start;justify-content:space-between;gap:8px}
.slot b{display:block;font-size:12px;margin-bottom:4px}
.slot-thumb{width:44px;height:33px;flex:0 0 auto;border:1px solid var(--line);border-radius:8px;background:linear-gradient(135deg,#f5f5f7,#e8e8ed);object-fit:cover}
.slot .actions{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:5px;margin-top:8px}
.slot button{height:30px;font-size:11px;padding:0 6px}
.tiny{font-size:11px;color:var(--muted)}
.value{font-size:12px;color:var(--ink);font-variant-numeric:tabular-nums}
@keyframes bluePulse{0%,100%{box-shadow:0 0 0 0 rgba(0,113,227,.32)}50%{box-shadow:0 0 0 5px rgba(0,113,227,.14)}}
@keyframes greenPulse{0%,100%{box-shadow:0 0 0 0 rgba(52,199,89,.42)}50%{box-shadow:0 0 0 6px rgba(52,199,89,.16)}}
@media(max-width:980px){header{padding:0 16px}main{grid-template-columns:1fr;padding:12px}.preview-wrap{height:62vh;order:-1}.panel{border-radius:18px}}
@media(max-width:520px){.row,.row3,.row4{grid-template-columns:1fr}.rotate-row{grid-template-columns:42px 42px minmax(0,1fr)}}
</style>
</head>
<body>
<header>
  <h1>ED060KD1 WiFi Uploader</h1>
  <div class="tiny">1448 x 1072 / 16 gray / 12 slots</div>
</header>
<main>
  <section class="panel stack">
    <div class="section">
      <p class="title">Connection</p>
      <div id="net" class="status">Loading</div>
      <div class="row" style="margin-top:8px">
        <div><label for="ssidList">Scanned SSID</label><select id="ssidList"><option value="">Scan first</option></select></div>
        <button class="secondary" id="wifiScan" style="margin-top:23px">Scan WiFi</button>
      </div>
      <div class="row" style="margin-top:8px">
        <div><label for="ssid">Router SSID</label><input id="ssid" type="text"></div>
        <div><label for="pass">Router Password</label><input id="pass" type="password"></div>
      </div>
      <div class="row" style="margin-top:8px">
        <button id="wifiSave">Connect STA</button>
        <button class="secondary" id="wifiClear">Clear STA</button>
      </div>
    </div>

    <div class="section">
      <p class="title">Image</p>
      <label for="file">Source</label>
      <input id="file" type="file" accept="image/*">
      <div class="row3" style="margin-top:8px">
        <div><label for="uploadSlot">Upload Slot</label><select id="uploadSlot"></select></div>
        <div><label for="showSlotSelect">Show Slot</label><select id="showSlotSelect"></select></div>
        <div><label for="fit">Fit</label><select id="fit"><option value="cover">Cover</option><option value="contain">Contain</option></select></div>
      </div>
      <div class="row3 rotate-row" style="margin-top:8px">
        <button class="secondary icon-btn" id="rotL" title="Rotate left">&#8634;</button>
        <button class="secondary icon-btn" id="rotR" title="Rotate right">&#8635;</button>
        <button class="secondary" id="resetCrop">Reset</button>
      </div>
    </div>

    <div class="section">
      <p class="title">Crop</p>
      <div class="row3">
        <div><label for="scale">Scale <span id="scaleV" class="value"></span></label><input id="scale" type="range" min="0.30" max="4" step="0.01" value="1"></div>
        <div><label for="offsetX">X <span id="offsetXV" class="value"></span></label><input id="offsetX" type="range" min="-1100" max="1100" step="1" value="0"></div>
        <div><label for="offsetY">Y <span id="offsetYV" class="value"></span></label><input id="offsetY" type="range" min="-900" max="900" step="1" value="0"></div>
      </div>
    </div>

    <div class="section">
      <p class="title">Tone</p>
      <div class="row3">
        <div><label for="brightness">Brightness <span id="brightnessV" class="value"></span></label><input id="brightness" type="range" min="-100" max="100" step="1" value="0"></div>
        <div><label for="contrast">Contrast <span id="contrastV" class="value"></span></label><input id="contrast" type="range" min="0.50" max="2.50" step="0.01" value="1"></div>
        <div><label for="gamma">Gamma</label><select id="gamma"><option value="1.0">1.0</option><option value="0.85">0.85</option><option value="1.2">1.2</option><option value="1.5">1.5</option></select></div>
      </div>
      <div class="row" style="margin-top:8px">
        <div><label for="algo">Algorithm</label><select id="algo"><option value="fs">Floyd-Steinberg</option><option value="level">Level</option></select></div>
        <label class="inline" style="margin-top:25px"><input id="clean" type="checkbox" checked> Clean refresh</label>
      </div>
      <div class="row3" style="margin-top:8px">
        <button id="process">Process</button>
        <button id="upload" disabled>Upload Slot</button>
        <button class="secondary" id="showSlot">Show Slot</button>
      </div>
    </div>

    <div class="section">
      <p class="title">Carousel</p>
      <div class="row">
        <label class="inline" style="margin-top:24px"><input id="carouselEnabled" type="checkbox"> Enabled</label>
        <div><label for="interval">Interval Seconds</label><input id="interval" type="number" min="10" max="86400" value="60"></div>
      </div>
      <div class="row" style="margin-top:8px">
        <div><label for="startupSlot">Startup Slot</label><select id="startupSlot"></select></div>
        <button class="secondary" id="startupSave" style="margin-top:23px">Save Startup</button>
      </div>
      <button id="carouselSave" style="width:100%;margin-top:8px">Save Carousel</button>
    </div>

    <div class="section">
      <p class="title">Developer</p>
      <div class="row4">
        <button class="secondary" id="devGray">16 Gray</button>
        <button class="secondary" id="devChecker">Checker</button>
        <button class="secondary" id="devResolution">Resolution</button>
        <button class="danger" id="devRepair">Repair</button>
      </div>
    </div>

    <div class="section">
      <p class="title">Slots</p>
      <div id="slots" class="slots"></div>
    </div>

    <div class="section">
      <div id="status" class="status">Ready</div>
      <div class="row" style="margin-top:8px">
        <button class="secondary" id="clear">Clear Screen</button>
        <button class="danger" id="reset">Restart ESP32</button>
      </div>
    </div>
  </section>
  <section class="preview-wrap">
    <canvas id="canvas" width="1448" height="1072"></canvas>
  </section>
</main>
<footer>copyright by imbread</footer>
<script>
const W=1448,H=1072,BYTES=W*H/2,SLOTS=12;
const EMPTY_THUMB='data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///ywAAAAAAQABAAACAUwAOw==';
const $=id=>document.getElementById(id);
const canvas=$('canvas');
const ctx=canvas.getContext('2d',{willReadFrequently:true});
let sourceImage=null;
let packed=null;
let crop={scale:1,x:0,y:0,rot:0};
let drag=null;
let slotsSnapshot=[];
let currentDisplayedSlot=-1;
let selectedUploadSlot=-1;
let startupSlot=-1;
let didAutoSelectUpload=false;
let pendingFileSlot=-1;
let pendingThumb=null;

function clamp(v,a,b){return Math.max(a,Math.min(b,v));}
function status(msg){$('status').textContent=msg;}
function syncValues(){
  $('scale').value=crop.scale;$('offsetX').value=crop.x;$('offsetY').value=crop.y;
  $('scaleV').textContent=Number(crop.scale).toFixed(2);
  $('offsetXV').textContent=Math.round(crop.x);
  $('offsetYV').textContent=Math.round(crop.y);
  $('brightnessV').textContent=$('brightness').value;
  $('contrastV').textContent=Number($('contrast').value).toFixed(2);
}

function slotLabel(slot){return String(Number(slot)+1).padStart(2,'0');}
function thumbKey(slot){return 'ed060kd1_thumb_'+slot;}
function getThumb(slot){try{return localStorage.getItem(thumbKey(slot));}catch(e){return null;}}
function setThumb(slot,data){try{localStorage.setItem(thumbKey(slot),data);}catch(e){}}
function clearThumb(slot){try{localStorage.removeItem(thumbKey(slot));}catch(e){}}

function updateSlotClasses(){
  document.querySelectorAll('.slot').forEach(el=>{
    const slot=Number(el.dataset.slot);
    el.classList.toggle('selected',slot===selectedUploadSlot);
    el.classList.toggle('current',slot===currentDisplayedSlot);
  });
}

async function setStartupSlot(slot,quiet=true){
  const n=Number(slot);
  if(!Number.isFinite(n)||n<-1||n>=SLOTS)return;
  startupSlot=n;
  if($('startupSlot'))$('startupSlot').value=n;
  updateSlotClasses();
  try{
    const r=await fetch('/startup-slot?slot='+n,{method:'POST'});
    const t=await r.text();
    if(!quiet)status(t);
  }catch(e){
    if(!quiet)status('Startup slot save failed');
  }
}

function selectSlot(slot,persist=false){
  selectedUploadSlot=Number(slot);
  $('uploadSlot').value=selectedUploadSlot;
  updateSlotClasses();
  if(persist)setStartupSlot(selectedUploadSlot,true);
}

function findNearestEmptySlot(startSlot){
  if(!slotsSnapshot.length)return 0;
  const start=Number.isFinite(Number(startSlot))?Number(startSlot):-1;
  for(let i=1;i<=SLOTS;i++){
    const idx=(start+i+SLOTS)%SLOTS;
    const item=slotsSnapshot.find(s=>s.id===idx);
    if(item&&!item.exists)return idx;
  }
  const first=slotsSnapshot.find(s=>!s.exists);
  return first?first.id:start>=0?start:0;
}

function autoSelectEmptySlot(startSlot){
  selectSlot(findNearestEmptySlot(startSlot));
}

function makeCanvasThumb(){
  const tw=96,th=72;
  const t=document.createElement('canvas');
  t.width=tw;t.height=th;
  const c=t.getContext('2d');
  c.fillStyle='#fff';c.fillRect(0,0,tw,th);
  c.drawImage(canvas,0,0,tw,th);
  return t.toDataURL('image/jpeg',0.72);
}

function makePackedThumb(buf){
  const tw=96,th=72;
  const t=document.createElement('canvas');
  t.width=tw;t.height=th;
  const c=t.getContext('2d');
  const img=c.createImageData(tw,th);
  for(let y=0;y<th;y++){
    const sy=Math.min(H-1,Math.floor(y*H/th));
    for(let x=0;x<tw;x++){
      const sx=Math.min(W-1,Math.floor(x*W/tw));
      const p=sy*W+sx;
      const b=buf[p>>1];
      const lvl=(p&1)?((b>>4)&15):(b&15);
      const q=lvl*17;
      const d=(y*tw+x)*4;
      img.data[d]=img.data[d+1]=img.data[d+2]=q;
      img.data[d+3]=255;
    }
  }
  c.putImageData(img,0,0);
  return t.toDataURL('image/jpeg',0.72);
}

async function ensureSlotThumb(slot){
  if(getThumb(slot))return;
  try{
    const r=await fetch('/slot/data?slot='+slot);
    if(!r.ok)return;
    const buf=new Uint8Array(await r.arrayBuffer());
    if(buf.length!==BYTES)return;
    const data=makePackedThumb(buf);
    setThumb(slot,data);
    const img=document.querySelector('.slot[data-slot="'+slot+'"] .slot-thumb');
    if(img)img.src=data;
  }catch(e){}
}

function drawEmpty(){
  ctx.fillStyle='#fff';ctx.fillRect(0,0,W,H);
  ctx.fillStyle='#111827';ctx.font='48px system-ui';ctx.textAlign='center';
  ctx.fillText('ED060KD1',W/2,H/2-30);
  ctx.font='28px system-ui';ctx.fillText('WiFi image uploader',W/2,H/2+30);
}

function drawSource(){
  ctx.fillStyle='#fff';ctx.fillRect(0,0,W,H);
  if(!sourceImage){drawEmpty();return;}
  const rad=crop.rot*Math.PI/180;
  const quarter=Math.abs(crop.rot)%180!==0;
  const rw=quarter?sourceImage.naturalHeight:sourceImage.naturalWidth;
  const rh=quarter?sourceImage.naturalWidth:sourceImage.naturalHeight;
  const fit=$('fit').value;
  const base=fit==='cover'?Math.max(W/rw,H/rh):Math.min(W/rw,H/rh);
  const s=base*crop.scale;
  ctx.save();
  ctx.translate(W/2+Number(crop.x),H/2+Number(crop.y));
  ctx.rotate(rad);
  ctx.drawImage(sourceImage,-sourceImage.naturalWidth*s/2,-sourceImage.naturalHeight*s/2,sourceImage.naturalWidth*s,sourceImage.naturalHeight*s);
  ctx.restore();
}

function resetCrop(){
  crop={scale:1,x:0,y:0,rot:0};
  syncValues();
  drawSource();
}

function updateFromInputs(){
  crop.scale=Number($('scale').value);
  crop.x=Number($('offsetX').value);
  crop.y=Number($('offsetY').value);
  syncValues();
  drawSource();
}

function processImage(){
  if(!sourceImage){status('Select an image first');return;}
  drawSource();
  const img=ctx.getImageData(0,0,W,H);
  const data=img.data;
  const brightness=Number($('brightness').value);
  const contrast=Number($('contrast').value);
  const gamma=Number($('gamma').value);
  const algo=$('algo').value;
  packed=new Uint8Array(BYTES);

  if(algo==='fs'){
    const lum=new Float32Array(W*H);
    for(let i=0,p=0;i<data.length;i+=4,p++){
      let r=(data[i]-128)*contrast+128+brightness;
      let g=(data[i+1]-128)*contrast+128+brightness;
      let b=(data[i+2]-128)*contrast+128+brightness;
      let y=0.299*clamp(r,0,255)+0.587*clamp(g,0,255)+0.114*clamp(b,0,255);
      lum[p]=255*Math.pow(clamp(y,0,255)/255,gamma);
    }
    for(let y=0;y<H;y++){
      for(let x=0;x<W;x++){
        const p=y*W+x;
        const old=clamp(lum[p],0,255);
        const lvl=clamp(Math.round(old*15/255),0,15);
        const q=lvl*17;
        const err=old-q;
        if(p&1)packed[p>>1]|=lvl<<4;else packed[p>>1]=lvl;
        const di=p*4;data[di]=data[di+1]=data[di+2]=q;data[di+3]=255;
        if(x+1<W)lum[p+1]+=err*7/16;
        if(y+1<H){
          if(x>0)lum[p+W-1]+=err*3/16;
          lum[p+W]+=err*5/16;
          if(x+1<W)lum[p+W+1]+=err/16;
        }
      }
      if((y&127)===0)status('Processing '+Math.round(y*100/H)+'%');
    }
  }else{
    for(let i=0,p=0;i<data.length;i+=4,p++){
      let r=(data[i]-128)*contrast+128+brightness;
      let g=(data[i+1]-128)*contrast+128+brightness;
      let b=(data[i+2]-128)*contrast+128+brightness;
      let y=0.299*clamp(r,0,255)+0.587*clamp(g,0,255)+0.114*clamp(b,0,255);
      y=255*Math.pow(clamp(y,0,255)/255,gamma);
      const lvl=clamp(Math.round(y*15/255),0,15);
      const q=lvl*17;
      if(p&1)packed[p>>1]|=lvl<<4;else packed[p>>1]=lvl;
      data[i]=data[i+1]=data[i+2]=q;data[i+3]=255;
    }
  }
  ctx.putImageData(img,0,0);
  pendingThumb=makeCanvasThumb();
  $('upload').disabled=false;
  status('Processed '+BYTES+' bytes');
}

function upload(){
  if(!packed){status('Process image first');return;}
  const slot=Number($('uploadSlot').value);
  const clean=$('clean').checked?'1':'0';
  const fd=new FormData();
  fd.append('frame',new Blob([packed],{type:'application/octet-stream'}),'slot.gray4');
  const xhr=new XMLHttpRequest();
  xhr.open('POST','/upload?format=gray4&slot='+slot+'&clean='+clean,true);
  xhr.upload.onprogress=e=>{if(e.lengthComputable)status('Uploading '+Math.round(e.loaded*100/e.total)+'%');};
  xhr.onload=()=>{
    const ok=xhr.status===200;
    status(ok?xhr.responseText:'Upload failed '+xhr.status);
    if(ok){
      setThumb(slot,pendingThumb||makeCanvasThumb());
      setStartupSlot(slot,true);
    }
    refreshSlots(ok?slot:null);
    pollStatus();
  };
  xhr.onerror=()=>status('Upload failed');
  xhr.send(fd);
}

async function post(url){
  const r=await fetch(url,{method:'POST'});
  const t=await r.text();
  status(t);
  refreshSlots();
  pollStatus();
}

async function refreshSlots(uploadCompletedSlot=null){
  const j=await fetch('/slots').then(r=>r.json());
  slotsSnapshot=j.slots||[];
  currentDisplayedSlot=Number.isFinite(Number(j.currentSlot))?Number(j.currentSlot):-1;
  startupSlot=Number.isFinite(Number(j.startupSlot))?Number(j.startupSlot):-1;
  $('carouselEnabled').checked=j.carousel.enabled;
  $('interval').value=j.carousel.interval;
  $('startupSlot').value=startupSlot;

  if(!didAutoSelectUpload){
    selectedUploadSlot=findNearestEmptySlot(-1);
    didAutoSelectUpload=true;
  }else if(uploadCompletedSlot!==null){
    selectedUploadSlot=findNearestEmptySlot(Number(uploadCompletedSlot));
  }else if(selectedUploadSlot<0){
    selectedUploadSlot=findNearestEmptySlot(-1);
  }
  $('uploadSlot').value=selectedUploadSlot;

  let html='';
  slotsSnapshot.forEach(s=>{
    if(!s.exists)clearThumb(s.id);
    const thumb=s.exists?(getThumb(s.id)||EMPTY_THUMB):EMPTY_THUMB;
    const cls='slot '+(s.exists?'filled ':'')+(s.id===selectedUploadSlot?'selected ':'')+(s.id===currentDisplayedSlot?'current ':'');
    html+='<div class="'+cls+'" data-slot="'+s.id+'" onclick="selectSlot('+s.id+',true)">';
    html+='<div class="slot-head"><div><b>'+slotLabel(s.id)+'</b><div class="tiny">'+(s.exists?Math.round(s.size/1024)+' KB':'Empty')+'</div></div>';
    html+='<img class="slot-thumb" alt="" src="'+thumb+'"></div>';
    html+='<div class="actions">';
    html+='<button class="secondary" onclick="event.stopPropagation();showSlot('+s.id+')">Show</button>';
    html+='<button class="secondary" onclick="event.stopPropagation();chooseSlotFile('+s.id+')">Up</button>';
    html+='<button class="danger" onclick="event.stopPropagation();deleteSlot('+s.id+')">Del</button>';
    html+='</div></div>';
  });
  $('slots').innerHTML=html;
  updateSlotClasses();
  slotsSnapshot.forEach(s=>{if(s.exists&&!getThumb(s.id))ensureSlotThumb(s.id);});
}

function showSlot(slot){
  $('showSlotSelect').value=slot;
  setStartupSlot(slot,true);
  post('/slot/show?slot='+slot+'&clean='+($('clean').checked?'1':'0'));
}

function chooseSlotFile(slot){
  selectSlot(slot,true);
  pendingFileSlot=Number(slot);
  $('file').value='';
  $('file').click();
}

function deleteSlot(slot){
  clearThumb(slot);
  if(currentDisplayedSlot===Number(slot))currentDisplayedSlot=-1;
  post('/slot/delete?slot='+slot);
}

async function pollStatus(){
  try{
    const j=await fetch('/status').then(r=>r.json());
    $('net').textContent='AP: ED060KD1-WIFI / open / '+j.ap+'\nSTA: '+(j.staConnected?j.sta:'not connected')+'\nSSID: '+(j.ssid||'-');
    if(Number.isFinite(Number(j.currentSlot))){
      currentDisplayedSlot=Number(j.currentSlot);
      updateSlotClasses();
    }
    if(Number.isFinite(Number(j.startupSlot))){
      startupSlot=Number(j.startupSlot);
      $('startupSlot').value=startupSlot;
    }
    status(j.message);
    if(j.busy)setTimeout(pollStatus,1500);
  }catch(e){}
}

async function scanWifi(){
  status('Scanning WiFi...');
  const btn=$('wifiScan');
  btn.disabled=true;
  try{
    const j=await fetch('/wifi/scan').then(r=>r.json());
    const list=$('ssidList');
    list.innerHTML='';
    const seen=new Set();
    j.networks.forEach(n=>{
      if(seen.has(n.ssid))return;
      seen.add(n.ssid);
      const label=n.ssid+'  '+n.rssi+' dBm  '+n.auth;
      list.add(new Option(label,n.ssid));
    });
    if(list.options.length===0)list.add(new Option('No networks found',''));
    if(list.value)$('ssid').value=list.value;
    status('Scan done: '+list.options.length+' SSID');
  }catch(e){
    status('WiFi scan failed');
  }finally{
    btn.disabled=false;
  }
}

function saveWifi(){
  const ssid=encodeURIComponent($('ssid').value);
  const pass=encodeURIComponent($('pass').value);
  post('/wifi?ssid='+ssid+'&pass='+pass);
}

function clearWifi(){
  $('ssid').value='';
  $('pass').value='';
  post('/wifi/clear');
}

function saveCarousel(){
  const en=$('carouselEnabled').checked?'1':'0';
  const interval=Math.max(10,Number($('interval').value)||60);
  post('/carousel?enabled='+en+'&interval='+interval);
}

function repairDisplay(){
  if(confirm('Repair mode will run 10 full black/white refresh cycles. Continue?')){
    post('/dev/repair');
  }
}

function canvasScale(e){
  const r=canvas.getBoundingClientRect();
  return {x:W/r.width,y:H/r.height};
}

canvas.addEventListener('mousedown',e=>{
  const s=canvasScale(e);
  drag={x:e.clientX,y:e.clientY,cx:crop.x,cy:crop.y,sx:s.x,sy:s.y};
});
window.addEventListener('mousemove',e=>{
  if(!drag)return;
  crop.x=clamp(drag.cx+(e.clientX-drag.x)*drag.sx,-1100,1100);
  crop.y=clamp(drag.cy+(e.clientY-drag.y)*drag.sy,-900,900);
  syncValues();
  drawSource();
});
window.addEventListener('mouseup',()=>drag=null);
canvas.addEventListener('wheel',e=>{
  e.preventDefault();
  const factor=e.deltaY<0?1.06:0.94;
  crop.scale=clamp(crop.scale*factor,0.30,4);
  syncValues();
  drawSource();
},{passive:false});

canvas.addEventListener('touchstart',e=>{
  if(e.touches.length!==1)return;
  const t=e.touches[0],s=canvasScale(t);
  drag={x:t.clientX,y:t.clientY,cx:crop.x,cy:crop.y,sx:s.x,sy:s.y};
},{passive:true});
canvas.addEventListener('touchmove',e=>{
  if(!drag||e.touches.length!==1)return;
  const t=e.touches[0];
  crop.x=clamp(drag.cx+(t.clientX-drag.x)*drag.sx,-1100,1100);
  crop.y=clamp(drag.cy+(t.clientY-drag.y)*drag.sy,-900,900);
  syncValues();
  drawSource();
},{passive:true});
canvas.addEventListener('touchend',()=>drag=null);

function init(){
  $('startupSlot').add(new Option('Auto',-1));
  for(let i=0;i<SLOTS;i++){
    const label='Slot '+String(i+1).padStart(2,'0');
    $('uploadSlot').add(new Option(label,i));
    $('showSlotSelect').add(new Option(label,i));
    $('startupSlot').add(new Option(label,i));
  }
  ['scale','offsetX','offsetY','brightness','contrast','gamma','fit','algo'].forEach(id=>$(id).addEventListener('input',id==='gamma'||id==='algo'?syncValues:updateFromInputs));
  $('brightness').addEventListener('input',syncValues);
  $('contrast').addEventListener('input',syncValues);
  $('file').onchange=()=>{
    const f=$('file').files[0];if(!f)return;
    const target=pendingFileSlot;
    pendingFileSlot=-1;
    const img=new Image();
    img.onload=()=>{
      sourceImage=img;
      packed=null;
      pendingThumb=null;
      $('upload').disabled=true;
      resetCrop();
      status(target>=0?'Image loaded for slot '+slotLabel(target):'Image loaded');
      URL.revokeObjectURL(img.src);
    };
    img.src=URL.createObjectURL(f);
  };
  $('rotL').onclick=()=>{crop.rot=(crop.rot+270)%360;drawSource();};
  $('rotR').onclick=()=>{crop.rot=(crop.rot+90)%360;drawSource();};
  $('resetCrop').onclick=resetCrop;
  $('process').onclick=()=>setTimeout(processImage,20);
  $('upload').onclick=upload;
  $('uploadSlot').onchange=()=>selectSlot($('uploadSlot').value,true);
  $('showSlotSelect').onchange=()=>selectSlot($('showSlotSelect').value,true);
  $('showSlot').onclick=()=>showSlot($('showSlotSelect').value);
  $('clear').onclick=()=>post('/clear');
  $('reset').onclick=()=>post('/reset');
  $('wifiScan').onclick=scanWifi;
  $('ssidList').onchange=()=>{$('ssid').value=$('ssidList').value;};
  $('wifiSave').onclick=saveWifi;
  $('wifiClear').onclick=clearWifi;
  $('carouselSave').onclick=saveCarousel;
  $('startupSave').onclick=()=>setStartupSlot($('startupSlot').value,false);
  $('startupSlot').onchange=()=>{
    const slot=Number($('startupSlot').value);
    if(slot>=0)selectSlot(slot,false);
    setStartupSlot(slot,false);
  };
  $('devGray').onclick=()=>post('/dev/grayscale');
  $('devChecker').onclick=()=>post('/dev/checker');
  $('devResolution').onclick=()=>post('/dev/resolution');
  $('devRepair').onclick=repairDisplay;
  syncValues();
  drawEmpty();
  refreshSlots();
  pollStatus();
  setInterval(()=>{pollStatus();refreshSlots();},8000);
}
init();
</script>
</body>
</html>
)rawliteral";

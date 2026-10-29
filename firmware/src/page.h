// Web page of the purifier (served by the ESP32).
#pragma once

static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Air Purifier</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--txt:#1d2330;--sub:#6b7280;--acc:#2563eb;--ok:#16a34a;--off:#9ca3af;--line:#e5e7eb}
@media (prefers-color-scheme:dark){:root{--bg:#111318;--card:#1b1f27;--txt:#e8eaf0;--sub:#9aa3b2;--acc:#5b8cff;--line:#2a303b}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--txt);font:16px/1.4 system-ui,sans-serif}
main{max-width:520px;margin:auto;padding:16px}h1{font-size:22px;margin:4px 0 16px}h2{font-size:16px;margin:0 0 12px}
.card{background:var(--card);border-radius:14px;padding:16px;margin-bottom:14px;box-shadow:0 1px 3px #0001}
.row{display:flex;gap:8px;flex-wrap:wrap}.stat{flex:1;min-width:90px}.stat b{display:block;font-size:20px}.stat span{color:var(--sub);font-size:13px}
button{font:inherit;border:1px solid var(--line);background:var(--card);color:var(--txt);border-radius:10px;padding:10px 12px;cursor:pointer;flex:1}
button.on{background:var(--acc);border-color:var(--acc);color:#fff}button.big{font-size:18px;padding:14px}
.ev{display:flex;gap:6px;align-items:center;flex-wrap:wrap;border-top:1px solid var(--line);padding:10px 0}
.days label{font-size:13px;display:inline-flex;align-items:center;gap:2px;margin-right:4px}
select,input{font:inherit;padding:6px;border-radius:8px;border:1px solid var(--line);background:var(--card);color:var(--txt)}
input[type=text],input[type=password]{width:100%;margin-bottom:8px}.sub{color:var(--sub);font-size:13px}
</style></head><body><main>
<h1>Air Purifier</h1>
<div class="card"><div class="row">
<div class="stat"><b id="st">–</b><span>Status</span></div>
<div class="stat"><b id="pm">–</b><span>Particles (PM2.5)</span></div>
<div class="stat"><b id="spd">–</b><span>Speed</span></div>
</div><p class="sub" id="info"></p>
<div class="row"><button class="big" id="bPower" onclick="sendCommand({power:!S.power})">Turn on</button></div></div>
<div class="card"><h2>Mode</h2><div class="row" id="modes"></div></div>
<div class="card"><h2>Light</h2><div class="row" id="lights"></div></div>
<div class="card"><h2>Schedule</h2><div id="sched"></div>
<div class="row" style="margin-top:8px"><button onclick="addEvent()">Add</button><button class="on" onclick="saveSchedule()">Save</button></div>
<p class="sub" id="schedMsg"></p></div>
<div class="card"><h2>Google Home (Sinric Pro)</h2><p class="sub" id="sinSt"></p>
<input type="text" id="sDev" placeholder="Device ID"><input type="text" id="sKey" placeholder="App Key">
<input type="password" id="sSecret" placeholder="App Secret">
<input type="text" id="sDevLight" placeholder="Light device ID (optional)">
<input type="text" id="sDevAir" placeholder="Air quality sensor device ID (optional)">
<label class="sub">Alert when particles exceed <input type="number" id="sThreshold" min="5" max="999" style="width:80px"></label><div class="row"><button onclick="saveSinric()">Save and restart</button></div></div>
</main><script>
const MODES=[[3,'Auto'],[1,'Silent'],[2,'Night'],[4,'Turbo']],LIGHTS=[[0,'On'],[1,'Dim'],[2,'Off']];
const DAYS=['Mo','Tu','We','Th','Fr','Sa','Su'];let S={},P=[];
const JSON_HEADERS={'Content-Type':'application/json'};
function buttons(id,list,field){const d=document.getElementById(id);d.innerHTML='';for(const[v,n]of list){const b=document.createElement('button');b.textContent=n;b.onclick=()=>sendCommand({[field]:v});b.dataset.v=v;d.appendChild(b)}}
buttons('modes',MODES,'mode');buttons('lights',LIGHTS,'light');
async function status(){try{S=await(await fetch('/api/status')).json()}catch(e){return}
st.textContent=S.valid?(S.power?'On':'Off'):'No data';pm.textContent=S.valid?S.pm25:'–';spd.textContent=S.power?S.speed:'–';
bPower.textContent=S.power?'Turn off':'Turn on';bPower.className='big'+(S.power?'':' on');
for(const b of modes.children)b.className=(S.power&&S.mode==b.dataset.v)?'on':'';
for(const b of lights.children)b.className=(S.light==b.dataset.v)?'on':'';
info.textContent='Time '+S.time+(S.lock?' · panel buttons locked':'');
sinSt.textContent=S.sinric.configured?(S.sinric.connected?'Connected':'Configured, not connected'):'Not configured';}
async function sendCommand(c){await fetch('/api/command',{method:'POST',headers:JSON_HEADERS,body:JSON.stringify(c)});setTimeout(status,800)}
function draw(){sched.innerHTML='';P.forEach((e,i)=>{const r=document.createElement('div');r.className='ev';
let h='<span class="days">'+DAYS.map((d,j)=>`<label><input type="checkbox" ${e.days>>j&1?'checked':''} onchange="P[${i}].days^=${1<<j}">${d}</label>`).join('')+'</span>';
h+=`<input type="time" value="${String(e.hour).padStart(2,'0')}:${String(e.minute).padStart(2,'0')}" onchange="const[a,b]=this.value.split(':');P[${i}].hour=+a;P[${i}].minute=+b">`;
h+=`<select onchange="P[${i}].action=+this.value">`+[[0,'Turn off']].concat(MODES.map(([v,n])=>[v,'Turn on: '+n])).map(([v,n])=>`<option value="${v}" ${e.action==v?'selected':''}>${n}</option>`).join('')+'</select>';
h+=`<button style="flex:0" onclick="P.splice(${i},1);draw()">✕</button>`;r.innerHTML=h;sched.appendChild(r)})}
function addEvent(){P.push({days:31,hour:8,minute:0,action:3});draw()}
async function saveSchedule(){const r=await fetch('/api/schedule',{method:'POST',headers:JSON_HEADERS,body:JSON.stringify(P)});schedMsg.textContent=r.ok?'Saved':'Could not save'}
async function saveSinric(){await fetch('/api/sinric',{method:'POST',headers:JSON_HEADERS,body:JSON.stringify({appKey:sKey.value,appSecret:sSecret.value,deviceId:sDev.value,lightDeviceId:sDevLight.value,airDeviceId:sDevAir.value,alertThreshold:+sThreshold.value})});sinSt.textContent='Restarting...'}
(async()=>{P=await(await fetch('/api/schedule')).json();draw();const s=await(await fetch('/api/sinric')).json();sKey.value=s.appKey;sDev.value=s.deviceId;sDevLight.value=s.lightDeviceId;sDevAir.value=s.airDeviceId;sThreshold.value=s.alertThreshold;if(s.secretSaved)sSecret.placeholder='App Secret (saved)'})();
status();setInterval(status,3000);
</script></body></html>)HTML";

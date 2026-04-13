#!/usr/bin/env python3
import socket, struct
from flask import Flask, request, jsonify, render_template_string

app = Flask(__name__)

SOCK   = "/tmp/rover_state.sock"
M_FMT  = "=i4xffiifiiiiiiii4xqq"
M_SIZE = struct.calcsize(M_FMT)  # 80
S_FMT  = "=ffiifiiiiiiii4xqq"
S_SIZE = struct.calcsize(S_FMT)  # 72

CMD_GET  = 1
CMD_SET  = 2
CMD_STOP = 3
CMD_MODE = 5

def _pack(cmd, temp=0.0, hum=0.0, light=0, motion=0, dist=0.0,
          fan=0, lon=0, heat=0, buzz=0,
          ml=0, mr=0, mstop=0, mode=0, sec=0, usec=0):
    return struct.pack(M_FMT, cmd,
                       float(temp), float(hum),
                       int(light), int(motion), float(dist),
                       int(fan), int(lon), int(heat), int(buzz),
                       int(ml), int(mr), int(mstop), int(mode),
                       int(sec), int(usec))

def _send(cmd, recv=False, **kw):
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect(SOCK)
        s.sendall(_pack(cmd, **kw))
        data = b""
        if recv:
            while len(data) < S_SIZE:
                chunk = s.recv(S_SIZE - len(data))
                if not chunk: break
                data += chunk
        s.close()
        return data
    except Exception as e:
        print(f"socket error: {e}")
        return b""

def get_state():
    data = _send(CMD_GET, recv=True)
    if len(data) != S_SIZE:
        return {"error": "no data"}
    v = struct.unpack(S_FMT, data)
    k = ["temperature","humidity","light_level","motion","distance",
         "fan_on","light_on","heater_on","buzzer_on",
         "motor_left","motor_right","motor_stop","mode",
         "updated_sec","updated_usec"]
    return dict(zip(k, v))

HTML = """<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Rover</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0a0a0a;color:#00ff88;font-family:monospace;
  display:flex;flex-direction:column;align-items:center;padding:20px}
h1{font-size:1.4em;margin-bottom:10px;letter-spacing:3px}
#status{background:#111;border:1px solid #00ff88;border-radius:8px;
  padding:10px 20px;margin-bottom:15px;width:100%;max-width:400px;
  font-size:.85em;line-height:1.8}
.sr{display:flex;justify-content:space-between}
.val{color:#fff}
.grid{display:grid;grid-template-columns:repeat(3,100px);
  grid-template-rows:repeat(3,100px);gap:10px;margin-bottom:15px}
.btn{background:#111;border:2px solid #00ff88;border-radius:12px;
  color:#00ff88;font-size:2em;cursor:pointer;display:flex;
  align-items:center;justify-content:center;user-select:none;
  -webkit-tap-highlight-color:transparent}
.btn:active{background:#00ff88;color:#000}
.empty{border-color:transparent!important;pointer-events:none}
#estop{background:#300;border:2px solid #f33;color:#f33;
  width:100%;max-width:320px;height:70px;font-size:1.2em;
  border-radius:12px;cursor:pointer;letter-spacing:2px;margin-bottom:15px}
#estop:active{background:#f33;color:#000}
.mrow{display:flex;gap:10px;margin-bottom:12px}
.mb{background:#111;border:1px solid #555;border-radius:8px;
  color:#888;padding:8px 16px;cursor:pointer;font-family:monospace}
.mb.on{border-color:#00ff88;color:#00ff88}
.srow{display:flex;align-items:center;gap:15px;margin-bottom:12px}
#spd{width:180px;accent-color:#00ff88}
#sv{color:#fff;min-width:30px}
#conn{font-size:.75em;margin-bottom:8px;color:#555}
#conn.ok{color:#00ff88}#conn.err{color:#f33}
</style>
</head>
<body>
<h1>[ ROVER CONTROL ]</h1>
<div id="conn">connecting...</div>
<div id="status">
  <div class="sr"><span>DIST</span>     <span class="val" id="sd">--</span></div>
  <div class="sr"><span>TEMP</span>     <span class="val" id="st">--</span></div>
  <div class="sr"><span>MODE</span>     <span class="val" id="sm">--</span></div>
  <div class="sr"><span>MOTOR L/R</span><span class="val" id="smot">--</span></div>
  <div class="sr"><span>E-STOP</span>   <span class="val" id="ses">--</span></div>
</div>
<div class="mrow">
  <button class="mb on" id="m0" onclick="setMode(0)">AUTO</button>
  <button class="mb"    id="m1" onclick="setMode(1)">MANUAL</button>
</div>
<div class="srow">
  <span>SPEED</span>
  <input type="range" id="spd" min="20" max="100" value="70"
    oninput="document.getElementById('sv').textContent=this.value">
  <span id="sv">70</span>
</div>
<div class="grid">
  <div class="btn empty"></div>
  <div class="btn" onpointerdown="mv('fwd')"   onpointerup="mv('stop')" onpointerleave="mv('stop')">&#9650;</div>
  <div class="btn empty"></div>
  <div class="btn" onpointerdown="mv('left')"  onpointerup="mv('stop')" onpointerleave="mv('stop')">&#9668;</div>
  <div class="btn"  onpointerdown="mv('stop')">&#9632;</div>
  <div class="btn" onpointerdown="mv('right')" onpointerup="mv('stop')" onpointerleave="mv('stop')">&#9658;</div>
  <div class="btn empty"></div>
  <div class="btn" onpointerdown="mv('bwd')"   onpointerup="mv('stop')" onpointerleave="mv('stop')">&#9660;</div>
  <div class="btn empty"></div>
</div>
<button id="estop" onclick="eStop()">!! EMERGENCY STOP !!</button>
<script>
const spd=()=>parseInt(document.getElementById('spd').value);
const post=(u,b)=>fetch(u,{method:'POST',
  headers:{'Content-Type':'application/json'},
  body:JSON.stringify(b)}).catch(()=>{});
function mv(dir){post('/api/move',{dir,speed:spd()});}
function eStop(){post('/api/estop',{});}
function setMode(m){
  [0,1].forEach(i=>document.getElementById('m'+i).classList.toggle('on',i===m));
  post('/api/mode',{mode:m});
}
function tick(){
  fetch('/api/state').then(r=>r.json()).then(d=>{
    const c=document.getElementById('conn');
    if(d.error){c.textContent='offline';c.className='err';return;}
    c.textContent='connected';c.className='ok';
    document.getElementById('sd').textContent=d.distance.toFixed(1)+' cm';
    document.getElementById('st').textContent=d.temperature.toFixed(1)+' C';
    document.getElementById('sm').textContent=['AUTO','MANUAL','GESTURE'][d.mode]??d.mode;
    document.getElementById('smot').textContent=d.motor_left+' / '+d.motor_right;
    document.getElementById('ses').textContent=d.motor_stop?'** ACTIVE **':'clear';
    [0,1].forEach(i=>document.getElementById('m'+i).classList.toggle('on',i===d.mode));
  }).catch(()=>{});
}
setInterval(tick,500);tick();
</script>
</body>
</html>"""

@app.route('/')
def index(): return render_template_string(HTML)

@app.route('/api/state')
def api_state(): return jsonify(get_state())

@app.route('/api/move', methods=['POST'])
def api_move():
    d   = request.get_json()
    dir_= d.get('dir','stop')
    s   = max(20, min(100, int(d.get('speed', 70))))
    m   = {'fwd':(s,s),'bwd':(-s,-s),'left':(-s,s),'right':(s,-s),'stop':(0,0)}
    ml, mr = m.get(dir_, (0,0))
    _send(CMD_SET, ml=ml, mr=mr, mstop=0, mode=1)
    return jsonify({'ok': True})

@app.route('/api/estop', methods=['POST'])
def api_estop():
    _send(CMD_STOP)
    return jsonify({'ok': True})

@app.route('/api/mode', methods=['POST'])
def api_mode():
    mode = int(request.get_json().get('mode', 0))
    _send(CMD_MODE, mode=mode)
    return jsonify({'ok': True, 'mode': mode})

if __name__ == '__main__':
    print(f"M_SIZE={M_SIZE} S_SIZE={S_SIZE}")
    app.run(host='0.0.0.0', port=5000, debug=False)

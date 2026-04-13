# -*- coding: utf-8 -*-
import socket, struct, subprocess, json, re, time, threading

SOCK  = "/tmp/rover_state.sock"
M_FMT = "=i4xffiifiiiiiiii4xqq"
S_FMT = "=ffiifiiiiiiii4xqq"
S_SIZE= struct.calcsize(S_FMT)
CMD_GET=1; CMD_SET=2; CMD_STOP=3; CMD_MODE=5
LLAMA = "/opt/llama.cpp/build/bin/llama-simple"
MODEL = "/opt/llama.cpp/models/tinyllama.gguf"

def _pack(cmd,temp=0.0,hum=0.0,light=0,motion=0,dist=0.0,fan=0,lon=0,heat=0,buzz=0,ml=0,mr=0,mstop=0,mode=0,sec=0,usec=0):
    return struct.pack(M_FMT,cmd,float(temp),float(hum),int(light),int(motion),float(dist),int(fan),int(lon),int(heat),int(buzz),int(ml),int(mr),int(mstop),int(mode),int(sec),int(usec))

def _send(cmd,recv=False,**kw):
    try:
        s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
        s.settimeout(2.0); s.connect(SOCK); s.sendall(_pack(cmd,**kw))
        data=b""
        if recv:
            while len(data)<S_SIZE:
                chunk=s.recv(S_SIZE-len(data))
                if not chunk: break
                data+=chunk
        s.close(); return data
    except Exception as e:
        print(f"[socket] {e}"); return b""

def get_state():
    data=_send(CMD_GET,recv=True)
    if len(data)!=S_SIZE: return None
    v=struct.unpack(S_FMT,data)
    k=["temperature","humidity","light_level","motion","distance","fan_on","light_on","heater_on","buzzer_on","motor_left","motor_right","motor_stop","mode","updated_sec","updated_usec"]
    return dict(zip(k,v))

def set_motors(ml,mr,mstop=0): _send(CMD_SET,ml=ml,mr=mr,mstop=mstop,mode=1)
def set_mode(m): _send(CMD_MODE,mode=m)
def emergency_stop(): _send(CMD_STOP)

def ask_llm(prompt,max_tokens=40):
    try:
        result=subprocess.run(
            [LLAMA,"-m",MODEL,"-n",str(max_tokens),"--temp","0.1","-p",prompt],
            capture_output=True,text=True,timeout=30)
        return result.stdout.strip()
    except Exception as e:
        print(f"[LLM] {e}"); return ""

def parse_action(text):
    m=re.search(r'\{.*?\}',text,re.DOTALL)
    if not m: return None
    try: return json.loads(m.group())
    except:
        try: return json.loads(re.sub(r',\s*}','}',m.group()))
        except: return None

def execute_action(action):
    if not action: return
    act=action.get("action","stop").lower()
    spd=max(0,min(100,int(action.get("speed",60))))
    print(f"[AI] {act} speed={spd}")
    if act=="forward": set_motors(spd,spd)
    elif act=="backward": set_motors(-spd,-spd)
    elif act=="turn_left": set_motors(-spd,spd)
    elif act=="turn_right": set_motors(spd,-spd)
    elif act in("stop","halt"): set_motors(0,0)
    elif act=="emergency_stop": emergency_stop()

def cmd_prompt(cmd):
    return f'Rover command "{cmd}". Reply with JSON only: {{"action":"'

def auto_prompt(dist):
    if dist<15:   return '{"action":"'
    elif dist<40: return '{"action":"'
    elif dist<60: return '{"action":"'
    else:         return '{"action":"'

KEYWORDS = {
    "forward":  ("forward",   70),
    "ahead":    ("forward",   70),
    "go":       ("forward",   70),
    "move":     ("forward",   70),
    "fast":     ("forward",   90),
    "quick":    ("forward",   90),
    "backward": ("backward",  60),
    "back":     ("backward",  60),
    "reverse":  ("backward",  60),
    "left":     ("turn_left", 60),
    "right":    ("turn_right",60),
    "stop":     ("stop",       0),
    "halt":     ("stop",       0),
    "brake":    ("stop",       0),
}

def keyword_match(user):
    words = user.lower().split()
    matched = None
    spd_override = None
    for w in words:
        if w in ("fast","quick"): spd_override = 90
        elif w in ("slow","slowly","gently"): spd_override = 30
        if w in KEYWORDS and matched is None:
            matched = KEYWORDS[w]
    if matched:
        act, spd = matched
        if spd_override: spd = spd_override
        return {"action": act, "speed": spd}
    return None

def auto_loop(stop_event):
    print("[AUTO] Running...")
    RULES = [(15,"stop",0),(40,"turn_right",50),(60,"forward",40),(999,"forward",70)]
    while not stop_event.is_set():
        state=get_state()
        if not state or state["mode"]==1:
            time.sleep(1); continue
        dist=state["distance"]
        for threshold, act, spd in RULES:
            if dist < threshold:
                execute_action({"action":act,"speed":spd})
                break
        time.sleep(3)

def cmd_loop():
    print("\n[CMD] Ready. Commands: 'auto','manual','stop','quit'\n")
    while True:
        try: user=input("rover> ").strip()
        except(EOFError,KeyboardInterrupt): break
        if not user: continue
        if user.lower()=="quit": break
        if user.lower()=="auto": set_mode(0); print("[CMD] AUTO"); continue
        if user.lower()=="manual": set_mode(1); print("[CMD] MANUAL"); continue
        if user.lower() in("stop","halt"): emergency_stop(); print("[CMD] STOP!"); continue

        action = keyword_match(user)
        if action:
            print(f"[CMD] {action['action']} speed={action['speed']}")
            execute_action(action)
            continue

        print("[AI] thinking...")
        response=ask_llm(cmd_prompt(user))
        action=parse_action(response)
        if action:
            if not action.get("speed"): action["speed"]=70
            execute_action(action)
        else:
            print(f"[AI] not understood: {user}")

if __name__=="__main__":
    print("=== Rover AI Brain ===")
    print("1=CMD  2=AUTO  3=BOTH")
    choice=input("Mode: ").strip()
    stop_event=threading.Event()
    if choice in("2","3"):
        threading.Thread(target=auto_loop,args=(stop_event,),daemon=True).start()
    if choice in("1","3"): cmd_loop()
    else:
        print("Ctrl+C to stop")
        try:
            while True: time.sleep(1)
        except KeyboardInterrupt: pass
    stop_event.set()
    print("=== Stopped ===")

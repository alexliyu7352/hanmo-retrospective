#!/usr/bin/env python3
"""Create original, optional low-volume instrumental ambience and UI sounds, with no sampled recordings."""
from pathlib import Path
import hashlib,json,wave
import numpy as np
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'game/assets/audio';RATE=22050

def wav(name,signal):
    """Save one mono PCM resource with a deterministic waveform and record its checksum."""
    path=OUT/name;data=(np.clip(signal,-1,1)*32767).astype('<i2').tobytes()
    with wave.open(str(path),'wb') as f:f.setnchannels(1);f.setsampwidth(2);f.setframerate(RATE);f.writeframes(data)
    return {'path':'assets/audio/'+name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'seconds':len(signal)/RATE}

def pluck(freq,seconds):
    """Synthesize a decaying harmonic string tone rather than reproducing a known tune or instrument sample."""
    t=np.arange(int(RATE*seconds))/RATE
    env=(1-np.exp(-t*100))*np.exp(-t*.80)
    wave=np.sin(2*np.pi*freq*t)+.27*np.sin(2*np.pi*freq*2*t)*np.exp(-t*1.3)+.14*np.sin(2*np.pi*freq*3*t)*np.exp(-t*2.3)
    return .22*env*wave

def main():
    """Write a 32-second original sparse phrase plus two short feedback sounds; all can be muted in game."""
    OUT.mkdir(parents=True,exist_ok=True);track=np.zeros(RATE*32)
    for at,freq in [(0,146.83),(3,220),(7,293.66),(10,329.63),(14,261.63),(18,220),(22,196),(26,146.83)]:
        note=pluck(freq,5);start=int(at*RATE);end=min(start+len(note),len(track));track[start:end]+=note[:end-start]
    t=np.arange(len(track))/RATE;track+=.008*np.sin(2*np.pi*73.415*t)*np.sin(np.pi*t/32)**2
    track[-RATE:]*=np.linspace(1,0,RATE);click=np.random.default_rng(50).normal(0,.18,int(RATE*.08))*np.exp(-np.arange(int(RATE*.08))/RATE*100)
    records=[wav('ambience.wav',track),wav('paper.wav',click),wav('confirm.wav',pluck(880,.35))]
    (OUT/'LICENSE.json').write_text(json.dumps({'origin':'Original procedural composition created for Hanmo; no external audio samples or melodies.','generator':'tools/compose_ambience.py','assets':records},ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()

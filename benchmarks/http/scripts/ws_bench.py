#!/usr/bin/env python3
import argparse, asyncio, json, statistics, time
import websockets

async def once(uri, count):
    samples=[]
    async with websockets.connect(uri) as ws:
        for _ in range(count):
            t=time.perf_counter(); await ws.send('hello'); await ws.recv()
            samples.append((time.perf_counter()-t)*1000)
    return samples

async def main(a):
    all_samples=[]
    for _ in range(a.repeats): all_samples += await once(a.uri, a.messages)
    elapsed=sum(all_samples)/1000
    print(json.dumps({'server':a.server,'scenario':'websocket','messages':len(all_samples),
                      'messages_per_second':len(all_samples)/elapsed if elapsed else 0,
                      'latency_mean_ms':statistics.mean(all_samples),
                      'latency_p50_ms':statistics.median(all_samples)}))

p=argparse.ArgumentParser(); p.add_argument('--uri',required=True); p.add_argument('--server',required=True); p.add_argument('--messages',type=int,default=1000); p.add_argument('--repeats',type=int,default=3)
a=p.parse_args(); asyncio.run(main(a))

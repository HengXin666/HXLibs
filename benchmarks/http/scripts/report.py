#!/usr/bin/env python3
"""Turn wrk_json.lua JSON-lines output into a self-contained HTML report."""
import argparse, html, json, pathlib, statistics

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--input', required=True); p.add_argument('--output', required=True)
    p.add_argument('--versions', required=True)
    for n in ('workers','wrk-threads','connections','duration','warmup','repeats','server-cpus','client-cpus'):
        p.add_argument('--'+n, required=True)
    a = p.parse_args(); rows=[]
    for line in pathlib.Path(a.input).read_text().splitlines():
        try:
            x=json.loads(line)
            if x.get('server'): rows.append(x)
        except json.JSONDecodeError: pass
    groups={}
    for x in rows: groups.setdefault(x['server'], []).append(x)
    summary=[]
    for name, xs in groups.items():
        rps=[float(x.get('requests_per_second', x.get('messages_per_second', 0))) for x in xs]
        lat=[float(x.get('latency_mean_us', x.get('latency_mean_ms', 0)*1000))/1000 for x in xs]
        summary.append((name, statistics.mean(rps) if rps else 0, statistics.mean(lat) if lat else 0, len(xs)))
    summary.sort(key=lambda x:x[1], reverse=True)
    trs=''.join(f'<tr><td>{html.escape(n)}</td><td>{r:,.2f}</td><td>{l:,.3f}</td><td>{c}</td></tr>' for n,r,l,c in summary)
    labels=json.dumps([x[0] for x in summary]); values=json.dumps([round(x[1],2) for x in summary])
    versions=html.escape(pathlib.Path(a.versions).read_text())
    meta='; '.join(f'{k}={getattr(a,k.replace("-","_"))}' for k in ('workers','wrk_threads','connections','duration','warmup','repeats','server_cpus','client_cpus'))
    out=f'''<!doctype html><meta charset="utf-8"><title>HXLibs HTTP benchmark</title>
<style>body{{font:15px system-ui;max-width:1000px;margin:2rem auto}}table{{border-collapse:collapse;width:100%}}td,th{{border:1px solid #ddd;padding:.5rem;text-align:right}}td:first-child,th:first-child{{text-align:left}}pre{{background:#f5f5f5;padding:1rem;white-space:pre-wrap}}</style>
<h1>HTTP benchmark</h1><p>Parameters: {html.escape(meta)}</p><canvas id="c" height="110"></canvas>
<table><tr><th>Server</th><th>Requests/sec (mean)</th><th>Latency ms (mean)</th><th>Runs</th></tr>{trs}</table><h2>Versions</h2><pre>{versions}</pre>
<script>const l={labels},v={values},c=document.getElementById('c'),x=c.getContext('2d');x.font='14px sans-serif';let m=Math.max(...v,1),w=c.width=c.clientWidth*2;c.height=220;v.forEach((n,i)=>{{let bw=w/v.length*.7;let h=n/m*160;let px=i*w/v.length+w/v.length*.15;x.fillStyle='#2563eb';x.fillRect(px,190-h,bw,h);x.fillStyle='#111';x.fillText(l[i],px,212);x.fillText(n.toFixed(0),px,180-h)}});</script>'''
    pathlib.Path(a.output).write_text(out)
if __name__ == '__main__': main()

#!/usr/bin/env python3
"""将 wrk JSON Lines 结果生成自包含的中文交互式 HTML 报告。"""

from __future__ import annotations

import argparse
import datetime as dt
import html
import json
import math
import pathlib
import statistics
from collections import defaultdict


SCENARIO_NAMES = {
    "hello": "短响应 / 低并发",
    "json-api": "JSON API / 标准并发",
    "route-query": "动态 URL 路由与参数解析",
    "html-page": "HTML 页面 / 标准并发",
    "payload-64k": "64 KiB 响应 / 带宽负载",
    "mixed-traffic": "混合流量 / 高并发",
    "websocket": "WebSocket 回显",
}


def percentile(values: list[float], percentage: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * percentage / 100
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)


def describe(values: list[float]) -> dict[str, float]:
    if not values:
        return {key: 0.0 for key in ("mean", "median", "min", "max", "p10", "p90", "stdev", "cv", "ci_low", "ci_high")}
    mean = statistics.mean(values)
    stdev = statistics.stdev(values) if len(values) > 1 else 0.0
    margin = 1.96 * stdev / math.sqrt(len(values)) if len(values) > 1 else 0.0
    return {
        "mean": mean,
        "median": statistics.median(values),
        "min": min(values),
        "max": max(values),
        "p10": percentile(values, 10),
        "p90": percentile(values, 90),
        "stdev": stdev,
        "cv": stdev / mean * 100 if mean else 0.0,
        "ci_low": max(0.0, mean - margin),
        "ci_high": mean + margin,
    }


def number(row: dict, key: str, fallback: float = 0.0) -> float:
    try:
        return float(row.get(key, fallback))
    except (TypeError, ValueError):
        return fallback


def load_rows(path: pathlib.Path) -> list[dict]:
    rows = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        try:
            row = json.loads(line)
        except json.JSONDecodeError:
            continue
        if not row.get("server"):
            continue
        row.setdefault("optimization", "运行时")
        row.setdefault("scenario", "未分类")
        row.setdefault("profile", "默认负载")
        row.setdefault("connections", 1 if row["scenario"] == "websocket" else 0)
        row["_line"] = line_number
        rows.append(row)
    return rows


def summarize(rows: list[dict]) -> list[dict]:
    groups: dict[tuple, list[dict]] = defaultdict(list)
    for row in rows:
        key = (
            str(row["server"]),
            str(row["optimization"]),
            str(row["scenario"]),
            str(row["profile"]),
            int(number(row, "connections")),
        )
        groups[key].append(row)

    result = []
    for (server, optimization, scenario, profile, connections), samples in groups.items():
        rps = [number(row, "requests_per_second", number(row, "messages_per_second")) for row in samples]
        bandwidth = [number(row, "bytes_per_second") for row in samples]

        def latency_ms(row: dict, percentile_name: str) -> float:
            us_key = f"latency_{percentile_name}_us"
            ms_key = f"latency_{percentile_name}_ms"
            if us_key in row:
                return number(row, us_key) / 1000
            if ms_key in row:
                return number(row, ms_key)
            if percentile_name == "p50":
                return number(row, "latency_mean_us") / 1000 or number(row, "latency_mean_ms")
            return 0.0

        latency = {
            name: statistics.median([latency_ms(row, name) for row in samples])
            for name in ("p50", "p90", "p99", "p999", "max")
        }
        errors = sum(int(number(row, "errors")) for row in samples)
        requests = sum(int(number(row, "requests", number(row, "messages"))) for row in samples)
        item = {
            "server": server,
            "optimization": optimization,
            "scenario": scenario,
            "scenario_name": SCENARIO_NAMES.get(scenario, scenario),
            "profile": profile,
            "connections": connections,
            "runs": len(samples),
            "rps": describe(rps),
            "bandwidth_mib": statistics.median(bandwidth) / 1024 / 1024 if bandwidth else 0,
            "latency": latency,
            "errors": errors,
            "requests": requests,
            "error_rate": errors / requests * 100 if requests else 0,
            "throughput_gap": 0.0,
            "latency_gap": 0.0,
            "o3_vs_o2": None,
        }
        result.append(item)

    comparison_groups: dict[tuple, list[dict]] = defaultdict(list)
    for item in result:
        comparison_groups[(item["scenario"], item["profile"], item["connections"])].append(item)
    for items in comparison_groups.values():
        best_rps = max((item["rps"]["median"] for item in items), default=0)
        positive_p99 = [item["latency"]["p99"] for item in items if item["latency"]["p99"] > 0]
        best_p99 = min(positive_p99, default=0)
        for item in items:
            item["throughput_gap"] = (best_rps - item["rps"]["median"]) / best_rps * 100 if best_rps else 0
            item["latency_gap"] = (item["latency"]["p99"] / best_p99 - 1) * 100 if best_p99 else 0

    o2_lookup = {
        (item["server"], item["scenario"], item["profile"], item["connections"]): item["rps"]["median"]
        for item in result if item["optimization"] == "O2"
    }
    for item in result:
        if item["optimization"] != "O3":
            continue
        baseline = o2_lookup.get((item["server"], item["scenario"], item["profile"], item["connections"]))
        if baseline:
            item["o3_vs_o2"] = (item["rps"]["median"] / baseline - 1) * 100

    return sorted(result, key=lambda item: (item["scenario"], -item["rps"]["median"], item["server"], item["optimization"]))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--versions", required=True)
    for name in (
        "workers", "wrk-threads", "connections", "low-connections", "high-connections",
        "duration", "warmup", "repeats", "server-cpus", "client-cpus",
    ):
        parser.add_argument(f"--{name}", required=True)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    input_path = pathlib.Path(args.input)
    rows = load_rows(input_path)
    summary = summarize(rows)
    versions = pathlib.Path(args.versions).read_text(encoding="utf-8")
    generated_at = dt.datetime.now().astimezone().strftime("%Y-%m-%d %H:%M:%S %Z")
    metadata = {
        "服务端 Worker": args.workers,
        "wrk 线程": args.wrk_threads,
        "连接范围": f"{args.low_connections} / {args.connections} / {args.high_connections}",
        "单轮采样": f"{args.duration} 秒",
        "预热": f"{args.warmup} 秒",
        "重复轮次": args.repeats,
        "服务端 CPU": args.server_cpus,
        "客户端 CPU": args.client_cpus,
    }
    payload = {
        "summary": summary,
        "metadata": metadata,
        "generated_at": generated_at,
        "sample_count": len(rows),
    }
    data_json = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")

    template = r'''<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="dark">
<title>HXLibs HTTP 性能实验室</title>
<style>
:root{--bg:#070b16;--panel:#101728cc;--panel2:#151e32;--line:#26324b;--text:#f4f7ff;--muted:#93a4c2;--cyan:#35d7ff;--violet:#9975ff;--green:#58e6a9;--amber:#ffc76a;--red:#ff6f91;--shadow:0 24px 70px #0007}*{box-sizing:border-box}html{scroll-behavior:smooth}body{margin:0;color:var(--text);font:14px/1.55 Inter,"Noto Sans SC","Microsoft YaHei",system-ui,sans-serif;background:radial-gradient(circle at 15% -5%,#183d68 0,transparent 32%),radial-gradient(circle at 90% 10%,#42277b 0,transparent 28%),var(--bg);min-height:100vh}body:before{content:"";position:fixed;inset:0;pointer-events:none;opacity:.16;background-image:linear-gradient(#91b7ff12 1px,transparent 1px),linear-gradient(90deg,#91b7ff12 1px,transparent 1px);background-size:44px 44px}.shell{width:min(1500px,calc(100% - 40px));margin:auto;padding:44px 0 70px}.hero{position:relative;overflow:hidden;padding:38px 40px;border:1px solid #5474a84d;border-radius:28px;background:linear-gradient(135deg,#13233ddd,#14152cdd);box-shadow:var(--shadow)}.hero:after{content:"";position:absolute;width:420px;height:420px;right:-140px;top:-190px;border-radius:50%;background:linear-gradient(135deg,var(--cyan),var(--violet));filter:blur(6px);opacity:.18}.eyebrow{display:flex;align-items:center;gap:10px;color:var(--cyan);font-size:12px;font-weight:800;letter-spacing:.18em;text-transform:uppercase}.pulse{width:9px;height:9px;border-radius:50%;background:var(--green);box-shadow:0 0 0 6px #58e6a91c,0 0 24px var(--green)}h1{position:relative;margin:15px 0 10px;font-size:clamp(34px,5vw,68px);line-height:1.05;letter-spacing:-.045em;background:linear-gradient(105deg,#fff 20%,#acdfff 55%,#c7b8ff);-webkit-background-clip:text;color:transparent}.subtitle{position:relative;max-width:820px;color:#b9c7dd;font-size:16px}.hero-meta{position:relative;display:flex;flex-wrap:wrap;gap:9px;margin-top:24px}.chip{padding:7px 11px;border:1px solid #637ba749;border-radius:99px;color:#c8d5e9;background:#0b132399}.section{margin-top:24px}.filters{position:sticky;z-index:20;top:12px;display:flex;flex-wrap:wrap;align-items:end;gap:12px;padding:15px 18px;border:1px solid var(--line);border-radius:18px;background:#0c1220e8;box-shadow:0 14px 40px #0006;backdrop-filter:blur(18px)}label{display:grid;gap:5px;color:var(--muted);font-size:11px;font-weight:700;letter-spacing:.06em}select{min-width:210px;padding:10px 36px 10px 12px;color:var(--text);border:1px solid #34435f;border-radius:10px;background:#121b2e;font:inherit;outline:none}select:focus{border-color:var(--cyan);box-shadow:0 0 0 3px #35d7ff1b}.hint{margin-left:auto;align-self:center;color:var(--muted)}.cards{display:grid;grid-template-columns:repeat(4,1fr);gap:14px}.card,.panel{border:1px solid var(--line);background:linear-gradient(145deg,#121a2bdd,#0d1423dd);box-shadow:0 18px 50px #0003}.card{padding:20px;border-radius:18px}.card-title{color:var(--muted);font-size:12px}.card-value{margin-top:7px;font-size:clamp(23px,2.3vw,34px);font-weight:800;letter-spacing:-.04em}.card-note{margin-top:5px;color:#7486a6;font-size:11px}.accent-cyan{color:var(--cyan)}.accent-green{color:var(--green)}.accent-violet{color:#b89dff}.accent-amber{color:var(--amber)}.grid{display:grid;grid-template-columns:1.4fr .8fr;gap:16px}.panel{padding:22px;border-radius:20px;overflow:hidden}.panel-head{display:flex;align-items:start;justify-content:space-between;gap:20px;margin-bottom:18px}.panel h2{margin:0;font-size:18px}.panel-desc{margin-top:3px;color:var(--muted);font-size:12px}.bars{display:grid;gap:11px}.bar-row{display:grid;grid-template-columns:minmax(150px,220px) 1fr 100px;align-items:center;gap:12px}.bar-label{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.bar-track{height:12px;overflow:hidden;border-radius:20px;background:#080d18}.bar-fill{height:100%;min-width:2px;border-radius:inherit;background:linear-gradient(90deg,var(--cyan),var(--violet));box-shadow:0 0 20px #35d7ff55}.bar-value{text-align:right;font-variant-numeric:tabular-nums}.deltas{display:grid;gap:10px}.delta{display:flex;justify-content:space-between;gap:14px;padding:13px;border:1px solid #26344d;border-radius:13px;background:#0c1321}.delta strong{font-size:17px}.positive{color:var(--green)}.negative{color:var(--red)}.neutral{color:var(--muted)}.table-wrap{overflow:auto;margin:0 -22px -22px;padding:0 22px 22px}table{width:100%;min-width:1440px;border-collapse:separate;border-spacing:0}th{position:sticky;top:0;padding:11px 10px;color:#8294b3;background:#101829;text-align:right;font-size:10px;letter-spacing:.05em;white-space:nowrap}th:first-child,th:nth-child(2){text-align:left}td{padding:13px 10px;border-top:1px solid #222d43;text-align:right;white-space:nowrap;font-variant-numeric:tabular-nums}td:first-child,td:nth-child(2){text-align:left}tbody tr:hover{background:#17213788}.rank{display:inline-grid;width:24px;height:24px;place-items:center;border-radius:7px;background:#1c2840;color:#b8c8e1}.server{font-weight:750}.opt{display:inline-block;margin-left:7px;padding:2px 7px;border:1px solid #3d5074;border-radius:6px;color:#a9bfe3;font-size:10px}.gap-best{color:var(--green)}.gap-mid{color:var(--amber)}.gap-high{color:var(--red)}.range-cell{min-width:160px}.range-line{position:relative;height:5px;margin:5px 0;border-radius:4px;background:#1d2940}.range-line i{position:absolute;height:100%;border-radius:inherit;background:var(--cyan)}.range-line b{position:absolute;top:-3px;width:3px;height:11px;border-radius:3px;background:#fff}.range-text{color:var(--muted);font-size:10px}.footer-grid{display:grid;grid-template-columns:1fr 1fr;gap:16px}.meta-list{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}.meta-item{padding:12px;border:1px solid #253149;border-radius:12px;background:#0b1220}.meta-key{color:var(--muted);font-size:10px}.meta-value{margin-top:3px;overflow-wrap:anywhere}pre{max-height:360px;overflow:auto;margin:0;padding:15px;color:#b9c9e1;border-radius:13px;background:#080d17;font:11px/1.6 "JetBrains Mono",monospace;white-space:pre-wrap}.empty{padding:50px;text-align:center;color:var(--muted)}@media(max-width:1000px){.cards{grid-template-columns:repeat(2,1fr)}.grid,.footer-grid{grid-template-columns:1fr}.hint{width:100%;margin:0}}@media(max-width:620px){.shell{width:min(100% - 20px,1500px);padding-top:15px}.hero{padding:27px 22px;border-radius:20px}.cards{grid-template-columns:1fr 1fr}.card{padding:15px}.filters{top:5px}select{min-width:0;width:100%}.filters label{flex:1 1 150px}.bar-row{grid-template-columns:110px 1fr 75px}.meta-list{grid-template-columns:1fr}}
</style>
</head>
<body>
<main class="shell">
  <header class="hero">
    <div class="eyebrow"><span class="pulse"></span>HXLibs Performance Lab</div>
    <h1>HTTP 性能实验室</h1>
    <div class="subtitle">Clang O2 / O3 双优化矩阵，覆盖短响应、JSON API、动态路由与参数解析、真实 HTML / 64 KiB 文件传输及混合流量。所有差距只在相同场景、负载和连接数内计算。</div>
    <div class="hero-meta" id="heroMeta"></div>
  </header>

  <section class="section filters">
    <label>测试场景<select id="scenarioSelect"></select></label>
    <label>负载范围<select id="profileSelect"></select></label>
    <label>编译优化<select id="optimizationSelect"><option value="all">全部配置</option><option>O2</option><option>O3</option><option>运行时</option></select></label>
    <div class="hint">排序：中位吞吐量 · 范围：跨重复轮次</div>
  </section>

  <section class="section cards">
    <article class="card"><div class="card-title">最高中位吞吐</div><div class="card-value accent-cyan" id="bestRps">—</div><div class="card-note" id="bestRpsName">—</div></article>
    <article class="card"><div class="card-title">最低 P99 延迟</div><div class="card-value accent-green" id="bestLatency">—</div><div class="card-note" id="bestLatencyName">—</div></article>
    <article class="card"><div class="card-title">累计完成请求</div><div class="card-value accent-violet" id="totalRequests">—</div><div class="card-note">当前筛选范围内所有轮次</div></article>
    <article class="card"><div class="card-title">累计错误率</div><div class="card-value accent-amber" id="errorRate">—</div><div class="card-note" id="errorCount">—</div></article>
  </section>

  <section class="section grid">
    <article class="panel">
      <div class="panel-head"><div><h2>吞吐量对比</h2><div class="panel-desc">中位 Requests/s；条形长度以当前第一名为 100%</div></div></div>
      <div class="bars" id="throughputBars"></div>
    </article>
    <article class="panel">
      <div class="panel-head"><div><h2>O3 相对 O2</h2><div class="panel-desc">同实现、同场景的中位吞吐变化</div></div></div>
      <div class="deltas" id="optimizationDeltas"></div>
    </article>
  </section>

  <section class="section panel">
    <div class="panel-head"><div><h2>完整统计与差距</h2><div class="panel-desc">同时查看吞吐离散区间、延迟分位数、稳定性和相对最优差距</div></div></div>
    <div class="table-wrap"><table><thead><tr>
      <th>#</th><th>实现 / 优化</th><th>连接</th><th>中位 RPS</th><th>P10–P90 RPS</th><th>最小–最大 RPS</th><th>均值 ± 标准差</th><th>CV</th><th>MiB/s</th><th>P50</th><th>P90</th><th>P99</th><th>P99.9</th><th>吞吐差距</th><th>P99 差距</th><th>错误率</th><th>轮次</th>
    </tr></thead><tbody id="resultBody"></tbody></table></div>
  </section>

  <section class="section footer-grid">
    <article class="panel"><div class="panel-head"><div><h2>实验参数</h2><div class="panel-desc">报告生成于 __GENERATED_AT__</div></div></div><div class="meta-list" id="metadata"></div></article>
    <article class="panel"><div class="panel-head"><div><h2>工具链与硬件</h2><div class="panel-desc">用于复现实验并判断跨机器结果是否可比</div></div></div><pre>__VERSIONS__</pre></article>
  </section>
</main>
<script id="benchmark-data" type="application/json">__DATA__</script>
<script>
const payload=JSON.parse(document.getElementById('benchmark-data').textContent),all=payload.summary;
const $=id=>document.getElementById(id),fmt=(n,d=0)=>Number(n||0).toLocaleString('zh-CN',{minimumFractionDigits:d,maximumFractionDigits:d});
const rps=n=>n>=1e6?(n/1e6).toFixed(2)+' M':n>=1e3?(n/1e3).toFixed(1)+' K':fmt(n,0);
const latency=n=>n<1?n.toFixed(3)+' ms':n<100?n.toFixed(2)+' ms':n.toFixed(1)+' ms';
const pct=n=>(n>=0?'+':'')+n.toFixed(1)+'%';
const scenarioSelect=$('scenarioSelect'),profileSelect=$('profileSelect'),optimizationSelect=$('optimizationSelect');
const scenarios=[...new Map(all.map(x=>[x.scenario,x.scenario_name])).entries()];
const initial=scenarios.some(x=>x[0]==='mixed-traffic')?'mixed-traffic':(scenarios[0]?.[0]||'');
scenarioSelect.innerHTML=scenarios.map(([id,name])=>`<option value="${id}" ${id===initial?'selected':''}>${name}</option>`).join('');
function updateProfiles(){const previous=profileSelect.value,profiles=[...new Set(all.filter(x=>x.scenario===scenarioSelect.value).map(x=>x.profile))],options=['全部负载（分别展示）',...profiles];profileSelect.innerHTML=options.map(x=>`<option>${x}</option>`).join('');profileSelect.value=options.includes(previous)?previous:options[0]}
function gapClass(n){return n<.05?'gap-best':n<15?'gap-mid':'gap-high'}
function render(){const aggregate=profileSelect.value==='全部负载（分别展示）';let rows=all.filter(x=>x.scenario===scenarioSelect.value&&(aggregate||x.profile===profileSelect.value)&&(optimizationSelect.value==='all'||x.optimization===optimizationSelect.value)).sort((a,b)=>b.rps.median-a.rps.median);
  const chips=Object.entries(payload.metadata).slice(0,6).map(([k,v])=>`<span class="chip">${k} · ${v}</span>`);$('heroMeta').innerHTML=chips.join('')+`<span class="chip">${payload.sample_count} 条有效样本</span>`;
  if(!rows.length){$('throughputBars').innerHTML='<div class="empty">当前筛选条件没有数据</div>';$('resultBody').innerHTML='';return}
  const best=rows[0],latRows=rows.filter(x=>x.latency.p99>0).sort((a,b)=>a.latency.p99-b.latency.p99),bestLat=latRows[0];
  const totalReq=rows.reduce((n,x)=>n+x.requests,0),totalErr=rows.reduce((n,x)=>n+x.errors,0),maxRps=best.rps.median||1;
  $('bestRps').textContent=rps(best.rps.median)+' req/s';$('bestRpsName').textContent=best.server+' · '+best.optimization;
  $('bestLatency').textContent=bestLat?latency(bestLat.latency.p99):'—';$('bestLatencyName').textContent=bestLat?bestLat.server+' · '+bestLat.optimization:'无延迟数据';
  $('totalRequests').textContent=rps(totalReq);$('errorRate').textContent=totalReq?(totalErr/totalReq*100).toFixed(4)+'%':'0%';$('errorCount').textContent=fmt(totalErr)+' 个错误';
  $('throughputBars').innerHTML=rows.map(x=>`<div class="bar-row"><div class="bar-label" title="${x.server} ${x.optimization}">${x.server} <span class="opt">${x.optimization}</span></div><div class="bar-track"><div class="bar-fill" style="width:${x.rps.median/maxRps*100}%"></div></div><div class="bar-value">${rps(x.rps.median)}</div></div>`).join('');
  const deltas=all.filter(x=>x.scenario===scenarioSelect.value&&x.profile===profileSelect.value&&x.optimization==='O3'&&x.o3_vs_o2!==null).sort((a,b)=>b.o3_vs_o2-a.o3_vs_o2);
  $('optimizationDeltas').innerHTML=deltas.length?deltas.map(x=>`<div class="delta"><span>${x.server}</span><strong class="${x.o3_vs_o2>0?'positive':x.o3_vs_o2<0?'negative':'neutral'}">${pct(x.o3_vs_o2)}</strong></div>`).join(''):'<div class="empty">该筛选范围没有 O2 / O3 配对数据</div>';
  $('resultBody').innerHTML=rows.map((x,i)=>{const left=x.rps.p10/maxRps*100,width=Math.max(.4,(x.rps.p90-x.rps.p10)/maxRps*100),med=x.rps.median/maxRps*100;return `<tr><td><span class="rank">${i+1}</span></td><td><span class="server">${x.server}</span><span class="opt">${x.optimization}</span></td><td>${fmt(x.connections)}</td><td><b>${fmt(x.rps.median)}</b></td><td class="range-cell"><div class="range-text">${fmt(x.rps.p10)} – ${fmt(x.rps.p90)}</div><div class="range-line"><i style="left:${left}%;width:${width}%"></i><b style="left:${med}%"></b></div></td><td>${fmt(x.rps.min)} – ${fmt(x.rps.max)}</td><td>${fmt(x.rps.mean)} ± ${fmt(x.rps.stdev)}</td><td class="${x.rps.cv<3?'gap-best':x.rps.cv<8?'gap-mid':'gap-high'}">${x.rps.cv.toFixed(2)}%</td><td>${fmt(x.bandwidth_mib,2)}</td><td>${latency(x.latency.p50)}</td><td>${latency(x.latency.p90)}</td><td>${latency(x.latency.p99)}</td><td>${latency(x.latency.p999)}</td><td class="${gapClass(x.throughput_gap)}">${x.throughput_gap<.05?'最优':'-'+x.throughput_gap.toFixed(1)+'%'}</td><td class="${gapClass(x.latency_gap)}">${x.latency_gap<.05?'最优':'+'+x.latency_gap.toFixed(1)+'%'}</td><td class="${x.error_rate===0?'gap-best':'gap-high'}">${x.error_rate.toFixed(4)}%</td><td>${x.runs}</td></tr>`}).join('');
}
scenarioSelect.addEventListener('change',()=>{updateProfiles();render()});profileSelect.addEventListener('change',render);optimizationSelect.addEventListener('change',render);
$('metadata').innerHTML=Object.entries(payload.metadata).map(([k,v])=>`<div class="meta-item"><div class="meta-key">${k}</div><div class="meta-value">${v}</div></div>`).join('');
updateProfiles();render();
</script>
</body></html>'''
    output = (template
        .replace("__DATA__", data_json)
        .replace("__VERSIONS__", html.escape(versions))
        .replace("__GENERATED_AT__", html.escape(generated_at)))
    output_path = pathlib.Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(output, encoding="utf-8")


if __name__ == "__main__":
    main()

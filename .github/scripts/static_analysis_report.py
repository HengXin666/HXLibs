#!/usr/bin/env python3
import argparse
import html
import json
import os
import re
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path


CLANG_RE = re.compile(
    r"^(?P<file>.*?):(?P<line>\d+):(?P<column>\d+):\s+"
    r"(?P<level>warning|error|note):\s+(?P<message>.*?)"
    r"(?:\s+\[(?P<check>[^\]]+)\])?$"
)

def normalize_path(value, root):
    if not value:
        return ""
    path = Path(html.unescape(value))
    try:
        if path.is_absolute():
            return str(path.resolve().relative_to(root))
    except ValueError:
        return str(path)
    return str(path)


def source_context(root, rel_file, line_no, radius):
    path = root / rel_file
    if not path.exists() or not path.is_file() or line_no <= 0:
        return []
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return []

    start = max(1, line_no - radius)
    end = min(len(lines), line_no + radius)
    return [
        {"line": idx, "text": lines[idx - 1], "hit": idx == line_no}
        for idx in range(start, end + 1)
    ]


def parse_clang_tidy(log_path, root):
    diagnostics = []
    for raw in log_path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = CLANG_RE.match(raw)
        if not match:
            continue
        item = match.groupdict()
        check = item.get("check") or ""
        if check in {"clang-diagnostic-error", "clang-diagnostic-note"}:
            category = check
        else:
            category = check or item["level"]
        diagnostics.append(
            {
                "tool": "clang-tidy",
                "file": normalize_path(item["file"], root),
                "line": int(item["line"]),
                "column": int(item["column"]),
                "severity": item["level"],
                "rule": category,
                "message": item["message"],
                "raw": raw,
            }
        )
    return diagnostics


def parse_cppcheck(log_path, root):
    diagnostics = []
    try:
        tree = ET.parse(log_path)
    except ET.ParseError:
        return diagnostics

    for error in tree.findall(".//error"):
        rule = error.attrib.get("id", "")
        severity = error.attrib.get("severity", "")
        message = error.attrib.get("msg", "")
        locations = error.findall("location")
        if not locations:
            diagnostics.append(
                {
                    "tool": "cppcheck",
                    "file": "",
                    "line": 0,
                    "column": 0,
                    "severity": severity,
                    "rule": rule,
                    "message": message,
                    "raw": ET.tostring(error, encoding="unicode"),
                }
            )
            continue

        loc = locations[0]
        diagnostics.append(
            {
                "tool": "cppcheck",
                "file": normalize_path(loc.attrib.get("file", ""), root),
                "line": int(loc.attrib.get("line") or 0),
                "column": int(loc.attrib.get("column") or 0),
                "severity": severity,
                "rule": rule,
                "message": message,
                "raw": ET.tostring(error, encoding="unicode"),
            }
        )
    return diagnostics


def severity_rank(severity):
    return {
        "error": 0,
        "warning": 1,
        "performance": 1,
        "portability": 2,
        "style": 3,
        "information": 4,
        "note": 5,
    }.get(severity, 6)


def enrich(diagnostics, root, context_lines):
    for diag in diagnostics:
        diag["context"] = source_context(root, diag["file"], diag["line"], context_lines)
    return sorted(
        diagnostics,
        key=lambda d: (
            severity_rank(d["severity"]),
            d["file"],
            d["line"],
            d["column"],
            d["rule"],
        ),
    )


def write_markdown(path, tool_name, diagnostics):
    by_severity = Counter(d["severity"] for d in diagnostics)
    by_rule = Counter(d["rule"] or "未分类" for d in diagnostics)
    by_file = Counter(d["file"] or "未知文件" for d in diagnostics)

    lines = [
        f"## {tool_name} 静态分析报告",
        "",
        f"- 诊断总数: **{len(diagnostics)}**",
    ]
    if diagnostics:
        lines.append(
            "- 严重级别: "
            + ", ".join(f"`{k}` {v}" for k, v in sorted(by_severity.items()))
        )
        lines.extend(["", "### 规则 Top 10", ""])
        lines.extend(f"- `{rule}`: {count}" for rule, count in by_rule.most_common(10))
        lines.extend(["", "### 文件 Top 10", ""])
        lines.extend(f"- `{file}`: {count}" for file, count in by_file.most_common(10))
        lines.extend(["", "### 前 20 条诊断", ""])
        lines.extend(
            f"- `{d['severity']}` `{d['rule'] or '未分类'}` "
            f"`{d['file']}:{d['line']}:{d['column']}` {d['message']}"
            for d in diagnostics[:20]
        )
    else:
        lines.append("- 未发现诊断。")
    lines.extend(["", "> 完整中文 HTML 报告请在本次 workflow 的 Artifacts 中下载。", ""])
    path.write_text("\n".join(lines), encoding="utf-8")


def render_html(tool_name, diagnostics):
    by_severity = Counter(d["severity"] for d in diagnostics)
    by_rule = Counter(d["rule"] or "未分类" for d in diagnostics)
    by_file = Counter(d["file"] or "未知文件" for d in diagnostics)

    def esc(value):
        return html.escape(str(value), quote=True)

    cards = "\n".join(
        f"""
        <article class="diag sev-{esc(d['severity'])}">
          <header>
            <div>
              <span class="badge">{esc(d['severity'])}</span>
              <span class="rule">{esc(d['rule'] or '未分类')}</span>
            </div>
            <a class="loc" href="#">{esc(d['file'])}:{d['line']}:{d['column']}</a>
          </header>
          <p class="message">{esc(d['message'])}</p>
          <pre class="context">{"".join(render_context_line(line) for line in d["context"]) or "无法读取源代码上下文"}</pre>
        </article>
        """
        for d in diagnostics
    )
    if not cards:
        cards = '<section class="empty">未发现诊断。</section>'

    return f"""<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{esc(tool_name)} 静态分析报告</title>
  <style>
    :root {{
      color-scheme: light;
      --bg: #f6f7f9;
      --panel: #ffffff;
      --text: #18202a;
      --muted: #657083;
      --line: #d8dde6;
      --accent: #1565c0;
      --error: #b42318;
      --warning: #a15c07;
      --info: #276749;
      --code-bg: #101828;
      --code-hit: #344054;
    }}
    * {{ box-sizing: border-box; }}
    body {{
      margin: 0;
      background: var(--bg);
      color: var(--text);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      line-height: 1.5;
    }}
    main {{ max-width: 1180px; margin: 0 auto; padding: 28px 18px 48px; }}
    h1 {{ margin: 0 0 8px; font-size: 28px; letter-spacing: 0; }}
    .subtitle {{ margin: 0 0 24px; color: var(--muted); }}
    .summary {{
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
      gap: 12px;
      margin-bottom: 18px;
    }}
    .metric, .panel {{
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 8px;
      padding: 14px;
    }}
    .metric strong {{ display: block; font-size: 28px; }}
    .metric span {{ color: var(--muted); }}
    .grid {{
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
      gap: 12px;
      margin-bottom: 18px;
    }}
    h2 {{ margin: 0 0 10px; font-size: 16px; }}
    ol {{ margin: 0; padding-left: 22px; }}
    li {{ margin: 4px 0; overflow-wrap: anywhere; }}
    .diag {{
      background: var(--panel);
      border: 1px solid var(--line);
      border-left: 5px solid var(--accent);
      border-radius: 8px;
      margin: 12px 0;
      padding: 14px;
    }}
    .sev-error {{ border-left-color: var(--error); }}
    .sev-warning, .sev-performance, .sev-portability, .sev-style {{ border-left-color: var(--warning); }}
    .sev-information, .sev-note {{ border-left-color: var(--info); }}
    .diag header {{
      display: flex;
      justify-content: space-between;
      gap: 10px;
      align-items: flex-start;
      flex-wrap: wrap;
    }}
    .badge {{
      display: inline-block;
      min-width: 72px;
      text-align: center;
      border-radius: 999px;
      padding: 2px 9px;
      color: #fff;
      background: #475467;
      font-size: 12px;
      font-weight: 700;
    }}
    .rule {{ margin-left: 8px; font-family: ui-monospace, SFMono-Regular, Consolas, monospace; color: var(--muted); }}
    .loc {{ color: var(--accent); text-decoration: none; overflow-wrap: anywhere; }}
    .message {{ margin: 10px 0 12px; font-weight: 600; }}
    .context {{
      margin: 0;
      padding: 12px;
      overflow-x: auto;
      border-radius: 6px;
      background: var(--code-bg);
      color: #e4e7ec;
      font: 13px/1.45 ui-monospace, SFMono-Regular, Consolas, monospace;
    }}
    .code-line {{ display: block; min-height: 19px; }}
    .hit {{ background: var(--code-hit); color: #fff; }}
    .ln {{ display: inline-block; width: 5ch; padding-right: 10px; color: #98a2b3; text-align: right; user-select: none; }}
    .empty {{ background: var(--panel); border: 1px solid var(--line); border-radius: 8px; padding: 24px; }}
  </style>
</head>
<body>
<main>
  <h1>{esc(tool_name)} 静态分析报告</h1>
  <p class="subtitle">报告包含诊断摘要、规则/文件聚合，以及每条诊断附近的源代码上下文。</p>
  <section class="summary">
    <div class="metric"><strong>{len(diagnostics)}</strong><span>诊断总数</span></div>
    <div class="metric"><strong>{len(by_file)}</strong><span>涉及文件</span></div>
    <div class="metric"><strong>{len(by_rule)}</strong><span>涉及规则</span></div>
  </section>
  <section class="grid">
    <div class="panel"><h2>严重级别</h2>{render_counter(by_severity)}</div>
    <div class="panel"><h2>规则 Top 10</h2>{render_counter(by_rule, 10)}</div>
    <div class="panel"><h2>文件 Top 10</h2>{render_counter(by_file, 10)}</div>
  </section>
  <section>{cards}</section>
</main>
</body>
</html>
"""


def render_counter(counter, limit=None):
    items = counter.most_common(limit)
    if not items:
        return "<p>无</p>"
    return "<ol>" + "".join(f"<li><code>{html.escape(k)}</code>: {v}</li>" for k, v in items) + "</ol>"


def render_context_line(line):
    cls = "code-line hit" if line["hit"] else "code-line"
    return (
        f'<span class="{cls}"><span class="ln">{line["line"]}</span>'
        f'{html.escape(line["text"])}</span>\n'
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool", choices=["clang-tidy", "cppcheck"], required=True)
    parser.add_argument("--input", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--repo-root", default=os.getcwd())
    parser.add_argument("--context-lines", type=int, default=4)
    args = parser.parse_args()

    root = Path(args.repo_root).resolve()
    log_path = Path(args.input)
    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.tool == "clang-tidy":
        diagnostics = parse_clang_tidy(log_path, root)
        title = "clang-tidy"
    else:
        diagnostics = parse_cppcheck(log_path, root)
        title = "cppcheck"

    diagnostics = enrich(diagnostics, root, args.context_lines)
    (out_dir / "diagnostics.json").write_text(
        json.dumps(diagnostics, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    (out_dir / "report.html").write_text(render_html(title, diagnostics), encoding="utf-8")
    write_markdown(out_dir / "summary.md", title, diagnostics)


if __name__ == "__main__":
    main()

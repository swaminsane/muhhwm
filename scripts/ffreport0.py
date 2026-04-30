#!/usr/bin/env python3
"""
muhhwm-analytics  —  Monthly Browser History Report Generator
Reads daily JSON history dumps and produces a designed HTML/PDF report.
"""

# ═══════════════════════════════════════════════════════════════════════════════
# SETTINGS  —  EDIT THESE BEFORE RUNNING
# ═══════════════════════════════════════════════════════════════════════════════

# Directory containing the daily .json files for the target month.
# Example structure:  ./2026/04/2026-04-02.json
INPUT_DIR = "/home/swaminsane/.local/share/firefox/history/2025/06"

# Year and month numbers (used for report titling and output filenames).
YEAR = 2025
MONTH = 6

# Where to write the generated report files.
OUTPUT_DIR = "/home/swaminsane/.local/share/firefox/report"
OUTPUT_HTML_NAME = f"{YEAR}-{MONTH:02d}-report.html"
OUTPUT_PDF_NAME = f"{YEAR}-{MONTH:02d}-report.pdf"

# Minutes of inactivity between two visits before a new session is started.
SESSION_GAP_MINUTES = 30

# Category detection rules.
# Format: list of (category_key, [domain_substrings, ...]).
# Checked in order; first match wins. Unmatched domains fall back to "other".
CATEGORY_RULES = [
    ("entertainment", ["youtube.com", "youtu.be", "miruro.tv", "pahe.win",
                       "kwik.cx", "thetvapp.to", "tvpass.org"]),
    ("research",      ["google.com", "bing.com", "duckduckgo.com", "archive.org",
                       "ukiyo-e.org", "data.ukiyo-e.org", "jaodb.com",
                       "nationalarchives.gov.uk", "lyx.org", "mfa.org",
                       "syngof.fr", "ukiyoe-gallery.com"]),
    ("work",          ["github.com", "gitlab.com", "localhost", "chatgpt.com",
                       "openai.com", "stackoverflow.com"]),
    ("admin",         ["bceceboard.bihar.gov.in", "admissions.nic.in",
                       "gov.in", "gov.uk"]),
]

# Colour for each category (CSS hex). "other" is auto-populated if omitted.
CATEGORY_COLORS = {
    "entertainment": "#c25e00",
    "research":      "#2e5aac",
    "work":          "#3a7d44",
    "admin":         "#8b6914",
    "other":         "#6b6b6b",
}

# Maximum number of sessions to render in the Session Archaeology block.
# A month can contain hundreds; cap keeps the PDF readable.
MAX_SESSIONS = 50

# Maximum visits shown per day in the heatmap before colour intensity caps.
HEATMAP_CAP = 50

# Whether to render cross-reference tags like [R-3.1] inside session chains
# and domain tables. The Reference Index is always generated.
INLINE_REFERENCES = True

# If True, attempt PDF generation via WeasyPrint after HTML is written.
# Requires:  pip install weasyprint
GENERATE_PDF = True

# If True and GENERATE_PDF is True, try to open the PDF when done (Linux).
AUTO_OPEN_PDF = False

# ═══════════════════════════════════════════════════════════════════════════════
# END OF SETTINGS
# ═══════════════════════════════════════════════════════════════════════════════


import json
import sys
from pathlib import Path
from collections import defaultdict, Counter
from datetime import datetime, timedelta

# ── Optional dependencies ─────────────────────────────────────────────────────
try:
    from jinja2 import Template
except ImportError as exc:
    raise SystemExit(
        "ERROR: jinja2 is required. Install it with:\n"
        "    pip install jinja2"
    ) from exc

try:
    from weasyprint import HTML
    WEASYPRINT_OK = True
except ImportError:
    WEASYPRINT_OK = False
    print("WARNING: WeasyPrint not found. PDF generation will be skipped.")
    print("Install it with:  pip install weasyprint")


# ═══════════════════════════════════════════════════════════════════════════════
# UTILITIES
# ═══════════════════════════════════════════════════════════════════════════════

def parse_ts(ts: str) -> datetime:
    """Parse ISO timestamp with timezone offset."""
    # fromisoformat handles +00:00 but not trailing 'Z' in older Pythons
    return datetime.fromisoformat(ts.replace("Z", "+00:00"))


def fmt_dur(seconds: float) -> str:
    """Human-friendly duration."""
    if seconds < 60:
        return f"{int(seconds)}s"
    if seconds < 3600:
        return f"{int(seconds // 60)}m {int(seconds % 60)}s"
    return f"{seconds / 3600:.1f}h"


def fmt_clock(dt: datetime) -> str:
    return dt.strftime("%H:%M")


# ═══════════════════════════════════════════════════════════════════════════════
# DATA LOADER
# ═══════════════════════════════════════════════════════════════════════════════

def load_entries(input_dir: Path):
    """
    Read every *.json in *input_dir*, flatten the nested visit structure,
    and return a single chronologically-sorted list of visit dicts.
    """
    entries = []
    if not input_dir.exists():
        raise SystemExit(f"ERROR: Input directory not found: {input_dir}")

    json_files = sorted(input_dir.glob("*.json"))
    if not json_files:
        raise SystemExit(f"ERROR: No .json files found in {input_dir}")

    for jfile in json_files:
        with open(jfile, "r", encoding="utf-8") as fh:
            data = json.load(fh)

        for rec in data.get("entries", []):
            base = {
                "url": rec.get("url", ""),
                "title": rec.get("title", "") or "",
                "domain": rec.get("domain", ""),
                "visit_count": rec.get("visit_count", 1),
                "typed_count": rec.get("typed_count", 0),
                "frecency": rec.get("frecency", 0),
            }
            for v in rec.get("visits", []):
                entries.append({
                    **base,
                    "visit_id": v["visit_id"],
                    "from_visit": v.get("from_visit", 0),
                    "timestamp_raw": v["timestamp"],
                    "timestamp": parse_ts(v["timestamp"]),
                    "visit_type": v.get("visit_type", "unknown"),
                })

    entries.sort(key=lambda x: x["timestamp"])
    return entries


def load_search_queries(input_dir: Path):
    """Aggregate search_queries arrays across all daily JSONs."""
    queries = []
    for jfile in sorted(input_dir.glob("*.json")):
        with open(jfile, "r", encoding="utf-8") as fh:
            data = json.load(fh)
        queries.extend(data.get("search_queries", []))
    return queries


# ═══════════════════════════════════════════════════════════════════════════════
# PROCESSORS
# ═══════════════════════════════════════════════════════════════════════════════

def categorize(domain: str) -> str:
    for cat, fragments in CATEGORY_RULES:
        if any(frag in domain for frag in fragments):
            return cat
    return "other"


def compute_dwell(entries: list):
    """
    Attach a 'dwell' field (seconds) to every entry.
    Dwell = time until next visit, capped at SESSION_GAP_MINUTES.
    Last visit in a chain gets 0.
    """
    gap_sec = SESSION_GAP_MINUTES * 60
    for i in range(len(entries)):
        if i + 1 < len(entries):
            diff = (entries[i + 1]["timestamp"] - entries[i]["timestamp"]).total_seconds()
            entries[i]["dwell"] = diff if diff < gap_sec else 0
        else:
            entries[i]["dwell"] = 0


def build_sessions(entries: list):
    """
    Split flat entries into sessions based on SESSION_GAP_MINUTES.
    Returns list of session lists.
    """
    sessions = []
    cur = []
    for e in entries:
        if not cur:
            cur.append(e)
            continue
        gap = (e["timestamp"] - cur[-1]["timestamp"]).total_seconds()
        if gap > SESSION_GAP_MINUTES * 60:
            sessions.append(cur)
            cur = [e]
        else:
            cur.append(e)
    if cur:
        sessions.append(cur)
    return sessions


def build_references(entries: list):
    """
    Build the R-x.y reference system.
    Returns:
        ref_map:  dict  url -> "R-x.y"
        domain_prefix: dict domain -> "R-x"
        domain_url_list: dict domain -> [url, ...]  (ordered by first appearance)
    """
    domain_urls = defaultdict(list)   # domain -> ordered unique urls
    domain_first = {}
    url_seen = set()

    for e in entries:
        d = e["domain"]
        u = e["url"]
        ts = e["timestamp"]
        if d not in domain_first or ts < domain_first[d]:
            domain_first[d] = ts
        if u not in url_seen:
            url_seen.add(u)
            domain_urls[d].append(u)

    # Domains ordered by first appearance
    sorted_domains = sorted(domain_first.keys(), key=lambda d: domain_first[d])

    ref_map = {}
    domain_prefix = {}
    domain_url_list = {}

    for idx, d in enumerate(sorted_domains, 1):
        prefix = f"R-{idx}"
        domain_prefix[d] = prefix
        domain_url_list[d] = domain_urls[d]
        for j, u in enumerate(domain_urls[d], 1):
            ref_map[u] = f"{prefix}.{j}"

    return ref_map, domain_prefix, domain_url_list


def aggregate_queries(queries_raw: list):
    """
    Deduplicate search queries by query string.
    Returns list of dicts: query, count, first_seen, last_seen, engine, domain
    """
    by_q = {}
    for q in queries_raw:
        text = q.get("query", "")
        if not text:
            continue
        ts = parse_ts(q["last_searched"])
        if text not in by_q:
            by_q[text] = {
                "query": text,
                "count": 0,
                "first_seen": ts,
                "last_seen": ts,
                "engine": q.get("engine", ""),
                "domain": q.get("domain", ""),
            }
        rec = by_q[text]
        rec["count"] += q.get("count", 1)
        if ts < rec["first_seen"]:
            rec["first_seen"] = ts
        if ts > rec["last_seen"]:
            rec["last_seen"] = ts

    return sorted(by_q.values(), key=lambda x: x["first_seen"])


# ═══════════════════════════════════════════════════════════════════════════════
# METRICS COMPILATION
# ═══════════════════════════════════════════════════════════════════════════════

def compile_metrics(entries, sessions, ref_map, domain_prefix):
    # ── Global ──
    total_visits = len(entries)
    unique_urls = len({e["url"] for e in entries})
    unique_domains = len({e["domain"] for e in entries})
    total_seconds = sum(e["dwell"] for e in entries)
    first_ts = entries[0]["timestamp"]
    last_ts = entries[-1]["timestamp"]

    # ── Daily pulse ──
    daily = Counter()
    for e in entries:
        daily[e["timestamp"].day] += 1
    max_day_visits = max(daily.values()) if daily else 1

    # ── Hourly heatmap (day x hour) ──
    # Build full month grid
    days_in_month = (last_ts.replace(day=28) + timedelta(days=4)).replace(day=1) - timedelta(days=1)
    days_in_month = days_in_month.day
    heatmap = {}
    for d in range(1, days_in_month + 1):
        heatmap[d] = {h: 0 for h in range(24)}
    for e in entries:
        heatmap[e["timestamp"].day][e["timestamp"].hour] += 1

    # ── Categories ──
    cat_stats = defaultdict(lambda: {"visits": 0, "seconds": 0})
    for e in entries:
        c = categorize(e["domain"])
        cat_stats[c]["visits"] += 1
        cat_stats[c]["seconds"] += e["dwell"]

    # ── Domain stats ──
    dom_stats = defaultdict(lambda: {
        "visits": 0, "dwell_total": 0, "dwell_n": 0,
        "first": None, "last": None, "titles": Counter(), "urls": set()
    })
    for e in entries:
        d = e["domain"]
        s = dom_stats[d]
        s["visits"] += 1
        if e["dwell"] > 0:
            s["dwell_total"] += e["dwell"]
            s["dwell_n"] += 1
        if s["first"] is None or e["timestamp"] < s["first"]:
            s["first"] = e["timestamp"]
        if s["last"] is None or e["timestamp"] > s["last"]:
            s["last"] = e["timestamp"]
        if e["title"]:
            s["titles"][e["title"]] += 1
        s["urls"].add(e["url"])

    domain_rows = []
    for d in sorted(dom_stats.keys(), key=lambda x: dom_stats[x]["visits"], reverse=True):
        s = dom_stats[d]
        avg_dwell = (s["dwell_total"] / s["dwell_n"]) if s["dwell_n"] else 0
        top_title = s["titles"].most_common(1)[0][0] if s["titles"] else ""
        # Build ref list string for this domain
        refs = [ref_map[u] for u in s["urls"] if u in ref_map]
        refs.sort()
        domain_rows.append({
            "domain": d,
            "visits": s["visits"],
            "time": fmt_dur(s["dwell_total"]),
            "avg_dwell": fmt_dur(avg_dwell),
            "window": f"{fmt_clock(s['first'])} – {fmt_clock(s['last'])}",
            "top_title": top_title[:45] + ("…" if len(top_title) > 45 else ""),
            "category": categorize(d),
            "refs": refs,
            "prefix": domain_prefix.get(d, ""),
        })

    # ── Sessions (rich objects) ──
    session_objs = []
    for sess in sessions:
        if not sess:
            continue
        start = sess[0]["timestamp"]
        end = sess[-1]["timestamp"]
        dur_sec = (end - start).total_seconds()
        cats = Counter(categorize(e["domain"]) for e in sess)
        main_cat = cats.most_common(1)[0][0] if cats else "other"
        # Build visit chain
        chain = []
        for e in sess:
            chain.append({
                "time": fmt_clock(e["timestamp"]),
                "domain": e["domain"],
                "title": (e["title"][:38] + "…") if len(e["title"]) > 38 else e["title"],
                "ref": ref_map.get(e["url"], ""),
                "vtype": e["visit_type"],
                "is_typed": e["visit_type"] == "typed",
            })
        session_objs.append({
            "start": fmt_clock(start),
            "end": fmt_clock(end),
            "date": start.strftime("%b %d"),
            "duration": fmt_dur(dur_sec),
            "category": main_cat,
            "chain": chain,
        })

    # Sort by duration desc, then keep top MAX_SESSIONS
    session_objs.sort(key=lambda x: (x["duration"], x["date"]), reverse=True)
    session_objs = session_objs[:MAX_SESSIONS]

    # ── Transitions ──
    trans = defaultdict(lambda: {"count": 0, "dwell_after": []})
    for sess in sessions:
        for i in range(len(sess) - 1):
            a = sess[i]["domain"]
            b = sess[i + 1]["domain"]
            if a == b:
                continue
            key = (a, b)
            trans[key]["count"] += 1
            trans[key]["dwell_after"].append(sess[i + 1]["dwell"])

    trans_rows = []
    for (a, b), data in sorted(trans.items(), key=lambda x: x[1]["count"], reverse=True):
        avg_after = sum(data["dwell_after"]) / len(data["dwell_after"]) if data["dwell_after"] else 0
        trans_rows.append({
            "from": a,
            "to": b,
            "count": data["count"],
            "avg_after": fmt_dur(avg_after),
            "from_ref": domain_prefix.get(a, ""),
            "to_ref": domain_prefix.get(b, ""),
        })

    return {
        "total_visits": total_visits,
        "unique_urls": unique_urls,
        "unique_domains": unique_domains,
        "total_time": fmt_dur(total_seconds),
        "first_date": first_ts.strftime("%Y-%m-%d"),
        "last_date": last_ts.strftime("%Y-%m-%d"),
        "daily": daily,
        "max_day_visits": max_day_visits,
        "days_in_month": days_in_month,
        "heatmap": heatmap,
        "categories": cat_stats,
        "domain_rows": domain_rows,
        "sessions": session_objs,
        "transitions": trans_rows,
    }


# ═══════════════════════════════════════════════════════════════════════════════
# JINJA2 TEMPLATE
# ═══════════════════════════════════════════════════════════════════════════════

HTML_TEMPLATE = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Digital Activity Report — {{ year }}-{{ "%02d"|format(month) }}</title>
<style>
@page {
  size: A4;
  margin: 1.8cm;
  @bottom-center {
    content: counter(page) " / " counter(pages);
    font: 8pt "Courier New", monospace;
    color: #888;
  }
}

:root {
  --bg: #faf9f7;
  --ink: #1a1a1a;
  --muted: #6b6b6b;
  --line: #d0d0d0;
  --ent: {{ colors.entertainment }};
  --res: {{ colors.research }};
  --work: {{ colors.work }};
  --adm: {{ colors.admin }};
  --oth: {{ colors.other }};
}

* { box-sizing: border-box; }
body {
  font: 9.5pt/1.45 "Helvetica Neue", Helvetica, Arial, sans-serif;
  color: var(--ink);
  background: #fff;
  margin: 0;
  padding: 0;
}

/* ── Cover ── */
.cover {
  height: 100vh;
  display: flex;
  flex-direction: column;
  justify-content: center;
  page-break-after: always;
}
.cover h1 {
  font: 700 38pt/1.1 "Helvetica Neue", sans-serif;
  letter-spacing: -0.03em;
  margin: 0;
}
.cover .subtitle {
  font: 12pt/1.5 "Courier New", monospace;
  color: var(--muted);
  margin-top: 1.2cm;
}
.cover .meta {
  margin-top: auto;
  font: 9pt "Courier New", monospace;
  color: var(--muted);
  border-top: 1px solid var(--line);
  padding-top: 0.4cm;
}

/* ── Sectioning ── */
section { page-break-inside: avoid; margin-bottom: 1.2cm; }
h2 {
  font: 700 15pt/1 "Helvetica Neue", sans-serif;
  letter-spacing: -0.02em;
  margin: 1.2cm 0 0.5cm;
  padding-bottom: 0.15cm;
  border-bottom: 2px solid var(--ink);
}
h3 {
  font: 600 10pt/1 "Helvetica Neue", sans-serif;
  color: var(--muted);
  text-transform: uppercase;
  letter-spacing: 0.06em;
  margin: 0.8cm 0 0.3cm;
}

/* ── Executive bars ── */
.bar-row {
  display: flex;
  align-items: center;
  margin: 0.12cm 0;
  font: 9pt "Courier New", monospace;
}
.bar-label { width: 3.2cm; text-align: right; padding-right: 0.35cm; }
.bar-track {
  flex: 1;
  height: 0.45cm;
  background: #e8e6e3;
  border-radius: 2px;
  overflow: hidden;
}
.bar-fill { height: 100%; border-radius: 2px; }
.bar-pct { width: 1.4cm; text-align: right; padding-left: 0.25cm; font-weight: 600; }
.bar-detail { width: 3.5cm; padding-left: 0.35cm; color: var(--muted); font-size: 7.5pt; }

/* ── Daily pulse ── */
.pulse-row {
  display: flex;
  align-items: center;
  font: 8.5pt "Courier New", monospace;
  margin: 0.08cm 0;
}
.pulse-day { width: 1.6cm; color: var(--muted); }
.pulse-track {
  flex: 1;
  height: 0.35cm;
  background: #e8e6e3;
  border-radius: 1px;
}
.pulse-fill { height: 100%; background: var(--ink); border-radius: 1px; }
.pulse-n { width: 2cm; text-align: right; padding-left: 0.3cm; }

/* ── Heatmap ── */
.heatmap { font: 7.5pt "Courier New", monospace; border-collapse: collapse; width: 100%; }
.heatmap th, .heatmap td {
  width: 0.55cm; height: 0.55cm;
  text-align: center;
  border: 1px solid #e0e0e0;
  padding: 0;
}
.heatmap th { background: #f0f0f0; font-weight: 400; }
.h0 { background: #fff; color: #ccc; }
.h1 { background: #ede8e0; }
.h2 { background: #d4c8b8; }
.h3 { background: #bba890; }
.h4 { background: #a28868; color: #fff; }
.h5 { background: #896840; color: #fff; }

/* ── Data tables ── */
.data {
  width: 100%;
  font: 9pt/1.35 "Helvetica Neue", sans-serif;
  border-collapse: collapse;
}
.data th {
  text-align: left;
  font-weight: 600;
  font-size: 7.5pt;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  color: var(--muted);
  padding: 0.18cm 0.12cm;
  border-bottom: 1px solid #bbb;
}
.data td {
  padding: 0.15cm 0.12cm;
  border-bottom: 1px solid #e8e8e8;
  vertical-align: top;
}
.data tr { page-break-inside: avoid; }

/* ── Sessions ── */
.session {
  border-left: 2.5px solid #ccc;
  padding-left: 0.45cm;
  margin: 0.35cm 0;
}
.session-header {
  font: 8.5pt "Courier New", monospace;
  color: var(--muted);
  margin-bottom: 0.15cm;
}
.session-tag {
  display: inline-block;
  padding: 0.04cm 0.18cm;
  border-radius: 2px;
  font: 7pt/1 "Helvetica Neue", sans-serif;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  margin-left: 0.2cm;
}
.visit-chain {
  font: 8pt/1.55 "Courier New", monospace;
  color: #333;
}
.visit-chain .t { color: var(--muted); width: 1.1cm; display: inline-block; }
.visit-chain .arr { color: #999; margin: 0 0.15cm; }
.visit-chain .typed { color: var(--res); }
.visit-chain .link { color: #777; }
.ref-tag {
  font-size: 7.5pt;
  color: var(--muted);
  margin-left: 0.15cm;
}

/* ── Transitions ── */
.trans { font: 9pt "Courier New", monospace; }
.trans td { padding: 0.12cm; }

/* ── Reference Index ── */
.ref-block { margin: 0.4cm 0; page-break-inside: avoid; }
.ref-domain {
  font: 10pt/1 "Helvetica Neue", sans-serif;
  font-weight: 600;
  margin-bottom: 0.15cm;
}
.ref-list {
  font: 8.5pt "Courier New", monospace;
  color: #333;
  padding-left: 0.4cm;
}
.ref-list div { margin: 0.06cm 0; }

/* ── Utilities ── */
.muted { color: var(--muted); }
.mono { font-family: "Courier New", monospace; }
.page-break { page-break-after: always; }
</style>
</head>
<body>

<!-- ═══════════════════════ COVER ═══════════════════════ -->
<div class="cover">
  <h1>Digital Activity<br>Report</h1>
  <div class="subtitle">
    {{ year }}–{{ "%02d"|format(month) }}<br>
    {{ total_visits }} visits · {{ unique_urls }} URLs · {{ unique_domains }} domains · {{ total_time }}
  </div>
  <div class="meta">
    Period {{ first_date }} → {{ last_date }}<br>
    Generated {{ generated_at }}<br>
    Firefox History Analytics
  </div>
</div>

<!-- ═══════════════════════ EXECUTIVE ═══════════════════════ -->
<section>
  <h2>Executive Summary</h2>
  {% for cat in cat_order %}
    {% set s = categories[cat] %}
    {% set pct = (s.visits / total_visits * 100)|round(0)|int if total_visits else 0 %}
    <div class="bar-row">
      <div class="bar-label" style="text-transform:capitalize;">{{ cat }}</div>
      <div class="bar-track">
        <div class="bar-fill" style="width:{{ pct }}%; background:{{ colors[cat] }};"></div>
      </div>
      <div class="bar-pct">{{ pct }}%</div>
      <div class="bar-detail">{{ s.visits }} visits, {{ fmt_dur(s.seconds) }}</div>
    </div>
  {% endfor %}
</section>

<!-- ═══════════════════════ DAILY PULSE ═══════════════════════ -->
<section>
  <h2>Daily Pulse</h2>
  {% for d in range(1, days_in_month+1) %}
    {% set n = daily.get(d, 0) %}
    {% set w = (n / max_day_visits * 100)|round(0)|int if max_day_visits else 0 %}
    <div class="pulse-row">
      <div class="pulse-day">{{ "%02d"|format(month) }}-{{ "%02d"|format(d) }}</div>
      <div class="pulse-track"><div class="pulse-fill" style="width:{{ w }}%;"></div></div>
      <div class="pulse-n">{{ n }} visit{{ "s" if n != 1 else "" }}</div>
    </div>
  {% endfor %}
</section>

<div class="page-break"></div>

<!-- ═══════════════════════ HOURLY HEATMAP ═══════════════════════ -->
<section>
  <h2>Hourly Heatmap</h2>
  <p class="muted" style="margin-bottom:0.3cm; font-size:8pt;">
    Each row is a day of the month. Darker cells = more visits in that hour.
  </p>
  <table class="heatmap">
    <tr>
      <th style="width:1.2cm;"></th>
      {% for h in range(24) %}<th>{{ "%02d"|format(h) }}</th>{% endfor %}
    </tr>
    {% for d in range(1, days_in_month+1) %}
    <tr>
      <th>{{ "%02d"|format(d) }}</th>
      {% for h in range(24) %}
        {% set n = heatmap[d][h] %}
        {% set cls = "h0" %}
        {% if n > 0 %}{% set cls = "h1" %}{% endif %}
        {% if n >= heatmap_cap * 0.25 %}{% set cls = "h2" %}{% endif %}
        {% if n >= heatmap_cap * 0.50 %}{% set cls = "h3" %}{% endif %}
        {% if n >= heatmap_cap * 0.75 %}{% set cls = "h4" %}{% endif %}
        {% if n >= heatmap_cap %}{% set cls = "h5" %}{% endif %}
        <td class="{{ cls }}">{{ n if n > 0 else "·" }}</td>
      {% endfor %}
    </tr>
    {% endfor %}
  </table>
</section>

<!-- ═══════════════════════ DOMAIN DEEP-DIVE ═══════════════════════ -->
<section>
  <h2>Domain Deep-Dive</h2>
  <table class="data">
    <tr>
      <th>Domain</th>
      <th>Visits</th>
      <th>Time</th>
      <th>Avg Dwell</th>
      <th>Primary Window</th>
      <th>Top Page</th>
      {% if inline_refs %}<th>Refs</th>{% endif %}
    </tr>
    {% for row in domain_rows %}
    <tr>
      <td><strong>{{ row.domain }}</strong></td>
      <td>{{ row.visits }}</td>
      <td>{{ row.time }}</td>
      <td>{{ row.avg_dwell }}</td>
      <td class="mono">{{ row.window }}</td>
      <td class="muted" style="font-size:8pt;">{{ row.top_title }}</td>
      {% if inline_refs %}
      <td class="mono" style="font-size:7.5pt;">
        {{ row.prefix }} ({{ row.refs|length }})
      </td>
      {% endif %}
    </tr>
    {% endfor %}
  </table>
</section>

<div class="page-break"></div>

<!-- ═══════════════════════ SESSION ARCHAEOLOGY ═══════════════════════ -->
<section>
  <h2>Session Archaeology</h2>
  <p class="muted" style="font-size:8pt; margin-bottom:0.4cm;">
    Top {{ sessions|length }} sessions by duration.
  </p>
  {% for sess in sessions %}
  <div class="session">
    <div class="session-header">
      {{ sess.date }} · {{ sess.start }} – {{ sess.end }} · {{ sess.duration }}
      <span class="session-tag" style="background:{{ colors[sess.category] }}22; color:{{ colors[sess.category] }};">
        {{ sess.category }}
      </span>
    </div>
    <div class="visit-chain">
      {% for v in sess.chain %}
      <div>
        <span class="t">{{ v.time }}</span>
        {% if loop.first %}▶{% else %}<span class="arr">→</span>{% endif %}
        <strong>{{ v.domain }}</strong>
        {% if v.title %}<span class="muted">{{ v.title }}</span>{% endif %}
        {% if inline_refs and v.ref %}<span class="ref-tag">[{{ v.ref }}]</span>{% endif %}
        <span class="{% if v.is_typed %}typed{% else %}link{% endif %}" style="font-size:7pt;">
          [{{ v.vtype }}]
        </span>
      </div>
      {% endfor %}
    </div>
  </div>
  {% endfor %}
</section>

<div class="page-break"></div>

<!-- ═══════════════════════ SEARCH QUERY LOG ═══════════════════════ -->
<section>
  <h2>Search Query Log</h2>
  <table class="data">
    <tr>
      <th>Query</th>
      <th>Engine</th>
      <th>First Seen</th>
      <th>Last Seen</th>
      <th>Times</th>
      {% if inline_refs %}<th>Ref</th>{% endif %}
    </tr>
    {% for q in queries %}
    <tr>
      <td><code>{{ q.query }}</code></td>
      <td>{{ q.engine }}</td>
      <td class="mono">{{ q.first_seen.strftime("%m-%d %H:%M") }}</td>
      <td class="mono">{{ q.last_seen.strftime("%m-%d %H:%M") }}</td>
      <td>{{ q.count }}</td>
      {% if inline_refs %}
      <td class="mono">{{ domain_prefix.get(q.domain, "") }}</td>
      {% endif %}
    </tr>
    {% endfor %}
  </table>
</section>

<!-- ═══════════════════════ DOMAIN TRANSITION WEB ═══════════════════════ -->
<section>
  <h2>Domain Transition Web</h2>
  <table class="data trans">
    <tr>
      <th>From</th>
      <th>→</th>
      <th>To</th>
      <th>Count</th>
      <th>Avg Dwell After</th>
      {% if inline_refs %}<th>Refs</th>{% endif %}
    </tr>
    {% for t in transitions %}
    <tr>
      <td>{{ t.from }}</td>
      <td>→</td>
      <td>{{ t.to }}</td>
      <td>{{ t.count }}</td>
      <td>{{ t.avg_after }}</td>
      {% if inline_refs %}
      <td class="mono">{{ t.from_ref }} → {{ t.to_ref }}</td>
      {% endif %}
    </tr>
    {% endfor %}
  </table>
</section>

<div class="page-break"></div>

<!-- ═══════════════════════ REFERENCE INDEX ═══════════════════════ -->
<section>
  <h2>Reference Index</h2>
  <p class="muted" style="font-size:8pt; margin-bottom:0.5cm;">
    Every unique URL encountered during the month, grouped by domain, ordered by first appearance.
  </p>
  {% for domain in ref_domains %}
  <div class="ref-block">
    <div class="ref-domain">
      {{ domain_prefix[domain] }} &nbsp;{{ domain }}
    </div>
    <div class="ref-list">
      {% for url in domain_url_list[domain] %}
      <div>{{ ref_map[url] }} &nbsp; {{ url }}</div>
      {% endfor %}
    </div>
  </div>
  {% endfor %}
</section>

</body>
</html>
"""


# ═══════════════════════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════════════════════

def main():
    input_path = Path(INPUT_DIR)
    out_dir = Path(OUTPUT_DIR)
    out_dir.mkdir(parents=True, exist_ok=True)
    out_html = out_dir / OUTPUT_HTML_NAME
    out_pdf = out_dir / OUTPUT_PDF_NAME

    print(f"Loading JSON files from {input_path} …")
    entries = load_entries(input_path)
    raw_queries = load_search_queries(input_path)
    print(f"  → {len(entries)} visits loaded")

    print("Computing metrics …")
    compute_dwell(entries)
    sessions = build_sessions(entries)
    ref_map, domain_prefix, domain_url_list = build_references(entries)
    metrics = compile_metrics(entries, sessions, ref_map, domain_prefix)
    queries = aggregate_queries(raw_queries)

    # Prepare category ordering for executive summary (by visit count desc)
    cat_order = sorted(
        metrics["categories"].keys(),
        key=lambda c: metrics["categories"][c]["visits"],
        reverse=True
    )

    # Ensure all category keys have a colour
    colors = dict(CATEGORY_COLORS)
    for c in cat_order:
        if c not in colors:
            colors[c] = colors.get("other", "#6b6b6b")

    ctx = {
        "year": YEAR,
        "month": MONTH,
        "generated_at": datetime.utcnow().strftime("%Y-%m-%d %H:%M UTC"),
        "total_visits": metrics["total_visits"],
        "unique_urls": metrics["unique_urls"],
        "unique_domains": metrics["unique_domains"],
        "total_time": metrics["total_time"],
        "first_date": metrics["first_date"],
        "last_date": metrics["last_date"],
        "daily": metrics["daily"],
        "max_day_visits": metrics["max_day_visits"],
        "days_in_month": metrics["days_in_month"],
        "heatmap": metrics["heatmap"],
        "heatmap_cap": HEATMAP_CAP,
        "categories": metrics["categories"],
        "cat_order": cat_order,
        "domain_rows": metrics["domain_rows"],
        "sessions": metrics["sessions"],
        "transitions": metrics["transitions"],
        "queries": queries,
        "colors": colors,
        "inline_refs": INLINE_REFERENCES,
        "ref_map": ref_map,
        "domain_prefix": domain_prefix,
        "domain_url_list": domain_url_list,
        "ref_domains": list(domain_url_list.keys()),
        "fmt_dur": fmt_dur,
    }

    print(f"Rendering HTML …")
    template = Template(HTML_TEMPLATE)
    html_body = template.render(ctx)

    with open(out_html, "w", encoding="utf-8") as fh:
        fh.write(html_body)
    print(f"  → {out_html}")

    if GENERATE_PDF:
        if not WEASYPRINT_OK:
            print("PDF generation skipped (WeasyPrint not installed).")
        else:
            print(f"Rendering PDF …")
            HTML(string=html_body).write_pdf(out_pdf)
            print(f"  → {out_pdf}")
            if AUTO_OPEN_PDF:
                import subprocess
                subprocess.run(["xdg-open", str(out_pdf)], check=False)

    print("Done.")


if __name__ == "__main__":
    main()

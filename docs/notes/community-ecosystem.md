# Community Ecosystem Design

## Investigation Templates

A bundled kit for a specific case type. Contains:

1. **Rule files** (.llama) — metadata + hash + signature + grep sections encoding investigative methodology
2. **Keyword files** — pattern lists for `-k` flag, tailored to case type
3. **SQL report queries** — case-type-specific analytics beyond defaults (run against Parquet output)
4. **Inclusion hash sets** — known-bad hashes relevant to case type
5. **Manifest** — `template.toml` declaring contents, required plugins, compatible versions, metadata
6. **Methodology README** — what the template looks for, how to interpret results, what to check manually

### Template Ideas by Case Type

- **Ransomware triage** — ransom note patterns, encrypted extensions, LOLBin signatures, recent executables, timeline queries
- **Insider threat / data exfil** — USB artifacts, cloud storage app signatures, archive creation, large file copy activity
- **Malware triage** — PE header anomalies, packer signatures, suspicious imports, known malware hashes, autorun locations
- **Browser forensics** — Chrome/Firefox/Edge SQLite DBs (via plugin), history/cookie/download patterns
- **Email investigation** — PST/OST/EML/EMLX signatures, email address regexes, attachment rules
- **Windows quick triage** — prefetch, event logs, registry hives, recent file access, scheduled tasks
- **IR IOC sweep** — convert STIX2/threat intel to llama rules + hash sets + keywords, run fast

## Rule Packs (more granular than templates)

- **MITRE ATT&CK technique packs** — one pack per technique (T1059, T1547, etc.)
- **OS artifact packs** — "Windows 11 artifacts," "macOS Ventura artifacts," versioned per OS release
- **Application-specific packs** — Slack, Teams, Signal, WhatsApp forensics
- **Threat intel packs** — regularly updated IOCs ("2026-Q1 ransomware IOCs," "APT-29 indicators")

## Distribution

**In-app package browser** (like Obsidian community plugins). Users never touch git. Browse, one-click install, auto-update. Backed by git repos under the hood.

- Browsable web registry (searchable, preview rules before install, download counts)
- Every registry page is a Google-indexable surface for "forensic X rules"
- Pinned versions per llama rule syntax / plugin API version
- Fork-and-customize workflow: analysts adapt templates, share back improvements

## Why This Is a Moat

X-Ways and Axiom have no community ecosystem. YARA has community rules but can't combine metadata filtering with content search — llama rules are strictly more expressive. Once there are 200 rule packs + 50 plugins + 30 templates, the ecosystem is the product. A competitor must replicate the community's accumulated forensic knowledge, not just the tool.

# GUI Product Strategy Notes

Market gap: Autopsy is dead, X-Ways is $1600/yr, Axiom is $3600/yr. Room for a fast, simple, cheaper forensic GUI.

## Growth Mechanisms

### Reports as Distribution

Every case produces reports seen by non-analysts (attorneys, HR, law enforcement) who have purchasing authority.

- Interactive HTML evidence packages (self-contained browser-viewable timeline/artifact explorer)
- Court-ready formatting (exhibit numbering, chain of custody blocks, Bates-style references)
- Subtle branding — every report is an ad

### Collaboration as Acquisition

- Shared case files — analyst A invites analyst B, who needs the tool (Figma/Slack loop)
- Portable `.llama` evidence containers bundling selected evidence + metadata + findings
- Export to TheHive / DFIR-IRIS / other case management tools

### Community as Moat

See [community-ecosystem.md](./community-ecosystem.md) for details.

- Shareable rule packs and investigation templates
- Plugin registry (WASM-sandboxed community plugins)
- Public browsable registry with one-click install from the app

### Free Tier as Growth Engine

- Free for individual analysts, paid for teams/orgs
- Gate on collaboration, audit trails, SSO, centralized rule management — not on case size
- Bottom-up enterprise sale: solo analysts adopt free, advocate when they join firms

### Analyst Mobility

- Portable personal config (rules, templates, layouts) — travels with analysts across jobs
- Certification program ("Llama Certified Examiner") — credential on LinkedIn = distribution
- Conference-friendly sanitized/redacted exports for presentations and blog posts

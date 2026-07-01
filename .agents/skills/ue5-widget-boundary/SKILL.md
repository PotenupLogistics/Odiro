---
name: ue5-widget-boundary
description: UE5 UMG/WBP ownership checks for C++ widget boundary work
---

# UE5 Widget Boundary

Use alongside `ue5-dev` for UE5 UMG/WBP work where WBP/C++ ownership affects Platform widgets, widget C++ review, or fallback UI risk.

## Boundary

- Keep WBP-owned: layout, styling, static labels/placeholders, icons/assets, animations, hover/press visuals, and design defaults.
- Keep C++-owned: runtime data/state derived from ViewModel, dynamic row/card creation, delegate wiring, and platform/system commands.
- Treat empty WBP-authored text/icon/style properties as intentional unless product requirements say otherwise; do not refill them from C++ fallback strings/icons.
- Prefer reusable WBP child widgets for repeated surfaces; C++ may instantiate configured widget classes but should not recreate their visual tree.

## Manual Scan

Run the bundled script on demand after Platform/UI widget C++ changes or during widget-boundary reviews:

```powershell
& .\.agents\skills\ue5-widget-boundary\scripts\check_widget_boundary.ps1
```

The script reports suspicious static copy, visual defaults, Tick usage, fallback widget tree construction, and C++ sizing/viewport breakpoint logic. Findings are review prompts and may be false positives; the default exit code remains success.
By default it scans Platform widget C++; pass `-ScanPath` when reviewing base UI widget paths too.

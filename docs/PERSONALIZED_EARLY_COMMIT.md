# Personalized early-commit confidence

Synced from TigerClaw main on 2026-09-18.

The native sentence decoder keeps two confidence views:

- `baseShare`: model-only confidence. Strong evidence (`0.999`), truncated-strong and boundary closure remain model-only.
- `share`: model confidence plus bounded personalization. Only ordinary multi-generation evidence (`0.99`) uses this value.

Supplement phrases contribute `min(0.75, supplementScore * 0.05)` to early-commit log confidence. Self-learning ranking uses persistent manual-correction levels: cross-context +6,+8,...,+24 and same-context +9,+11,...,+27. Learning bonus is capped at 0.75 and total personalization at 0.80.

Only an explicit manual correction from the current first choice to another candidate creates or strengthens learning. Normal acceptance of an already learned top candidate and automatic early commits never reinforce learning. Learning has no time decay. A learning-affected generation that is also beam-truncated remains ineligible for automatic commit. Empty-code auto commit remains model-only.

Product early-commit thresholds are aligned with TigerClaw: ordinary `0.99`, strong `0.999`, boundary closure `0.99999`, three ordinary generations or two strong generations, and retain three raw keys. Current-generation truncated evidence is retained but must satisfy model-only strong evidence.

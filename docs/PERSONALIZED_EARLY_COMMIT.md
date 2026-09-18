# Personalized early-commit confidence

Synced from TigerClaw main on 2026-09-18.

The native sentence decoder keeps two confidence views:

- `baseShare`: model-only confidence. Strong evidence (`0.999`), truncated-strong and boundary closure remain model-only.
- `share`: model confidence plus bounded personalization. Only ordinary multi-generation evidence (`0.99`) uses this value.

Supplement phrases contribute `min(0.75, supplementScore * 0.05)` to early-commit log confidence. Self-learning keeps Tigirl's existing ranking score curve and maps it to maturity without changing ranking semantics: the first stable observation contributes 0, the second about 0.5 maturity, and the third about 1.0. Learning bonus is capped at 0.75 and total personalization at 0.80.

Explicit user acceptance of an already learned top candidate may reinforce the preference until it is effectively mature. Automatic early commits never reinforce learning. A learning-affected generation that is also beam-truncated remains ineligible for automatic commit. Empty-code auto commit remains model-only.

Product early-commit thresholds are aligned with TigerClaw: ordinary `0.99`, strong `0.999`, boundary closure `0.99999`, three ordinary generations or two strong generations, and retain three raw keys. Current-generation truncated evidence is retained but must satisfy model-only strong evidence.

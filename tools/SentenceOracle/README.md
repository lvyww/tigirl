# Sentence reference oracle

This is a separate snapshot of the current supplied TigerClaw source for the
non-Qwen sentence port. `upstream-sha256.json` records exact original bytes.
Do not modify upstream files to make parity pass. Some archived source files
mention neural reranking/lifecycle for scope analysis; they are not runtime
components of the native input method.

The .NET console project compiles the original n-gram reader, sentence decoder
and supporting index classes with minimal test adapters. It reads a model and
UTF-16-hex query file and writes original scores and observed-bigram results.
It does not activate TSF, access registry configuration, run a model service or
change the reference checkout. Build outputs remain under ignored bin/obj.

`tests/sentence_ngram_test.py` uses this oracle and native probes against the
same real model. The `--lexicon` entry point compares original index preparation and queries.
`--supplement` compares serialized supplementary automata.
`--decoder <fixture> <model-or-dash> <0-full-or-1-incremental>` runs the frozen
decoder, candidate-existence queries and prefix evidence. The original rank
resource is embedded with its exact logical resource name.

`tests/sentence_decoder_test.py` compares neutral and real-model full/incremental
results on ARM64, x64 and x86. Fixtures cover rank-vs-score ordering, merged path
mass, short-symbol placement, numeric/punctuation selectors, malformed selectors,
supplements, isolation penalties, incomplete-tail evidence, prefix constraints,
beam truncation, appends/deletes, resets and changes to the candidate limit and
evidence request. This is decoder parity, not installed engine acceptance.
Original committed golden traces and engine trace adapters remain to be wired.

`--supplement-file <directory>` uses the frozen original supplement-loading and
comment-stripping method bodies with a UTF-8 fixture adapter. It is used by
`tests/sentence_resources_test.py` to verify production import/resource loading.

# GP200 integration provenance

GP200NAM.cpp/.h, GP200ModSyncProtocol.h and Eligible.h originate from the supplied GP200 NAM A2 Lite handoff (2026-10-18), derived from the GP200 VST editor 0.1.14. GP200Panel adapts its REVIEW3 loader. Preserve the inherited GPL-3.0 terms in COPYING.GPL-3.0 when distributing this integration. The upstream Tone3000 MIT license remains intact; it does not relicense these imported portions.

readJson is a memory-input adapter to the original readWeights parser. Weight ordering and NH04 framing remain unchanged. No firmware binary is included or changed.

# JavaScript Emitter (`js_emitter.py`) Architecture & V8 Optimizations

This document explains how LemmaJet's optimized JavaScript (ES Module) emitter implements the [Universal 4-Stage Inference Pipeline](model-inference-and-bridge-architecture.md) and why it achieves a **2.2x–2.75x pure uncached speedup**, **3.0x–4.4x Zipf stream speedup**, and **10x–43x warm batch speedup** in V8 / Node.js without increasing file size.

> **Distribution Notice**:
> This document is bundled directly into the standalone JavaScript / npm package distribution (`pkg/js/docs/javascript-emitter-optimizations.md`) to provide search and frontend engineers with full visibility into the flat TypedArray automaton and V8 JIT optimizations powering LemmaJet.

---

## 1. Why the Baseline JavaScript Code Bottlenecked in V8

In the baseline `js_emitter.py`, the trie was stored as nested JavaScript object literals (`const _ROOT = {a:{b:1,$:2},...}`) and walked via:

```js
for (let i = wLen - 1; i >= 0; i--) {
  let next = curr[w[i]];
  if (next === undefined) break;
  if (typeof next === "number") {
    ...
    break;
  }
  if (Array.isArray(next)) {
    const pfx = next[0];
    const pLen = pfx.length;
    if (i < pLen || !w.startsWith(pfx, i - pLen)) break;
    ...
  }
  curr = next;
  const op = curr.$;
  if (op !== undefined) {
    ...
  }
}
```

### Four V8 / JIT Killers in the Baseline Loop
1. **Megamorphic Hidden Classes (V8 Maps / Shapes)**:
   - V8 accelerates object property accesses using Inline Caches (ICs) keyed on an object's *Hidden Class* (the exact sequence of property names on that object).
   - In a suffix trie with 15,000+ internal nodes, almost every node has a different subset of letter keys (`{a, e, $}`, `{i, o, y}`, `{c, k, ł, ż}`, etc.).
   - Consequently, `_ROOT` creates **thousands of distinct Hidden Classes**. The property access `curr[w[i]]` and `curr.$` immediately degrades from monomorphic IC to **megamorphic / dictionary property lookup**, hashing property strings and chasing heap pointers on every step.
2. **Per-Character Heap String Allocation (`w[i]`)**:
   - Indexing a string `w[i]` in JavaScript creates a 1-character string value on every loop iteration just to pass as a property key into `curr[...]`.
3. **Heterogeneous Value Types (`number` vs `Array` vs `Object`)**:
   - Every transition target `next` can be a primitive SMI (`number`), a 1-element or 2-element `Array` (DAG ref or radix edge), or a plain `Object` (branch node). V8's TurboFan JIT must emit type-tag checks (`typeof next === "number"`, `Array.isArray(next)`) on every step.
4. **Closure Allocation in `words.map(stem)`**:
   - `Array.prototype.map` invokes `stem(element, index, array)` with 3 arguments through an higher-order callback dispatch.

---

## 2. The 6 Core JavaScript / V8 Optimizations

### Optimization 1: Keep Ultra-Compact `_RAW` on Disk, Compile to Flat `TypedArray`s at Load Time
How do we get the L1-cache speed of a flat `Int32Array` state machine **without** inflating the `.js` file size on disk?

- **On Disk (`_OPS`, `_RAW`, `_RAWDAG`)**:
  - We emit an ultra-compact literal format:
    - Unquoted identifier keys (`{a:1,b:2,$:0}` instead of `{"a":1,"b":2,"":0}`).
    - Deduplicated `_OPS = [[cut, "add"], ...]` table indexed by negative integers (`-1 - i`).
    - Radix multi-character edges (`["prefix", child]`).
  - As a result, the optimized JS module on Polish `md-balanced` is **287.8 KB** (within 9 KB of baseline `278.8 KB`, and **145 KB smaller** than naive JSON serialization!).
- **At Module Load Time (~6–11 ms on EN, ~37–85 ms on PL)**:
  - A compact ~70-line initializer walks `_RAW` once, expands radix edges and DAG refs, assigns integer state IDs `0 .. S-1`, and populates contiguous TypedArrays (`_CID`, `_NEXT`, `_NEED`, `_OP`, `_OP_CUT`, `_OP_BR`).
  - Once built, the intermediate node graph is garbage-collected, leaving only flat contiguous arrays in memory!

### Optimization 2: Character-Code Alphabet Compression (`_CID = new Uint8Array(512)`)
- Instead of `w[i]` (which allocates a 1-char string), we read the UTF-16 code unit as a raw integer via **`w.charCodeAt(i)`** (1 CPU instruction in V8 TurboFan!).
- Across all Tier-1 languages (`en, pl, de, es, fr, cs`), every lowercase letter in ASCII + Latin-1 + Latin Extended-A/B has code point **$< 512$** (e.g. Polish `'ż'` is `U+017C = 380`, Czech `'ř'` is `U+0159 = 345`).
- At load time, we scan all characters present in `_ROOT` and `_PFX_TRIE` and assign each unique character a compact sequential ID `1 .. A` (typically $A \le 38$, always $< 64$):
  ```js
  const _CID = new Uint8Array(512);
  // _CID[code] -> 1..A (or 0 if character never appears in the trie)
  ```
- At runtime:
  ```js
  const code = w.charCodeAt(i);
  const cid = code < 512 ? _CID[code] : 0;
  if (cid === 0) break;
  ```
  Any character not in the trie alphabet (digits, punctuation, out-of-alphabet symbols, or non-matching letters) exits in **one bounds-checked byte load**!

### Optimization 3: Power-of-2 Stride-64 State Transition Table (`(state << 6) | cid`)
- Every state `s` (`0 <= s < _NUM_STATES`) has a 64-slot row in a single flat `Int32Array`:
  ```js
  const _NEXT = new Int32Array(_NUM_STATES * 64);
  ```
- Why stride `64`?
  - Because `64 = 2^6`, computing the index for `(state, cid)` uses a **1-cycle bitwise shift and OR**:
    ```js
    const nxt = _NEXT[(state << 6) | cid];
    ```
  - No integer multiplication (`state * stride`), no object pointer chasing, no V8 Hidden Classes, and sequential memory layout in L1/L2 CPU cache!

### Optimization 4: Bitwise NOT Leaf Early-Exit (`~childId`)
- In a suffix trie, **60% to 75% of all edges point to leaf states** (nodes that have an operation `op`, but **zero** outgoing child transitions).
- In a standard state machine, transitioning to a leaf state `childId` sets `state = childId`, updates `bestOp`, decrements `i--`, reads `w.charCodeAt(i)` on the next iteration, looks up `_NEXT[(childId << 6) | cid]`, gets `0`, and finally `break`s.
- **Our Bitwise NOT Trick**:
  - At load time, when building `_NEXT`, we check whether `child` has any outgoing edges (`hasKids`):
    ```js
    _NEXT[base | cid] = hasKids ? childId : ~childId;
    ```
  - Since `childId >= 1`, `~childId = -childId - 1` is **strictly negative (`< 0`)**, while `0` means "no transition (dead end)" and `> 0` means "internal state with children"!
  - At runtime:
    ```js
    const nxt = _NEXT[(state << 6) | cid];
    if (nxt <= 0) {
      if (nxt < 0) {
        const s = ~nxt;
        if (_NEED[s] <= wLen) bestOp = _OP[s];
      }
      break;
    }
    state = nxt;
    if (_NEED[state] <= wLen) bestOp = _OP[state];
    ```
  - **Impact**: Every time a word hits a leaf node in the trie, `nxt < 0` recovers the leaf state via `const s = ~nxt`, checks `_NEED[s] <= wLen`, and **immediately `break`s out of the loop**—eliminating an entire extra loop iteration and `charCodeAt` lookup!

### Optimization 5: Flat `TypedArray` Prefix Automaton (`_PFX_NEXT`, `_PFX_NEED`, `_PFX_OP`)
- Rather than walking `_PFX_TRIE` with slow object lookups (`currP[w[i]]`), we compile `_PFX_TRIE` into the exact same flat `Int32Array` / `Uint8Array` format using the shared `_CID` table, guarded by a compile-time first-character check (`w.charCodeAt(0) === 110` for Polish `'n'`):
  ```js
  if (w.length > _AFF_MIN && w.charCodeAt(0) === 110) {
    let stateP = 0, bestP = 0;
    const pLen = w.length;
    for (let j = 0; j < pLen; j++) {
      const c = w.charCodeAt(j);
      const cid = c < 512 ? _CID[c] : 0;
      if (cid === 0) break;
      const nxtP = _PFX_NEXT[(stateP << 6) | cid];
      if (nxtP <= 0) {
        if (nxtP < 0) {
          const s = ~nxtP;
          if (_PFX_NEED[s] <= pLen) bestP = _PFX_OP[s];
        }
        break;
      }
      stateP = nxtP;
      if (_PFX_NEED[stateP] <= pLen) bestP = _PFX_OP[stateP];
    }
    if (bestP > 0) {
      w = w.slice(bestP);
      ...
    }
  }
  ```

### Optimization 6: `_OP_BR` Stem-Bridge Filter, Pre-Seeded `Map` Cache & Preallocated Batch Loop
- **`_OP_BR = new Uint8Array(numOps)`**:
  - Precomputes `1` if `add === ""` or any key in `_EXC` ends with `add`, else `0`.
  - At Stage 3.5, `if (_OP_BR[bestOp] === 1)` skips `_EXC[w]` whenever the alternation suffix `add` cannot possibly match an exception key.
- **Pre-Seeded Bounded `Map` Cache (`_CACHE`)**:
  - Initialized with `new Map(Object.entries(_EXC))` plus `_CACHE.set("", "")`.
  - Just like Python, checking `_CACHE.get(word)` at entry handles cached words, lowercase surface exceptions, and `""` in a single lookup—and skips Stage 1 `_EXC[w]` when `w === word`!
- **Preallocated `new Array(n)` Batch Loop**:
  - Replaces `words.map(stem)` with `const n = words.length, out = new Array(n); for (let k = 0; k < n; k++) ...` with the cache hit path inlined directly inside the `for` loop.

---

## 3. Annotated Walkthrough of the Emitted JS Hot Loop

```js
export function stem(word) {
  if (!word) return "";
  // 1. Pre-seeded Map cache hit (covers seen words + lowercase Stage 1 _EXC)
  let r = _CACHE.get(word);
  if (r !== undefined) return r;

  let w = word.toLowerCase();
  // Only re-check cache/exceptions if input had uppercase characters
  if (w !== word) {
    const r2 = _CACHE.get(w);
    if (r2 !== undefined) {
      if (_CACHE.size >= _CACHE_MAX) _resetCache();
      _CACHE.set(word, r2);
      return r2;
    }
  }
  if (w.length < 2 || w.length >= 255) {
    if (_CACHE.size >= _CACHE_MAX) _resetCache();
    _CACHE.set(word, w);
    return w;
  }

  // 2. Stage 2: Flat TypedArray Prefix Automaton (with charCodeAt(0) guard)
  if (w.length > _AFF_MIN && w.charCodeAt(0) === 110) {
    ...
  }

  // 3. Stage 3: Flat TypedArray Reversed Suffix Automaton
  const wLen = w.length;
  let bestOp = _NEED[0] <= wLen ? _OP[0] : -1;
  let state = 0;
  for (let i = wLen - 1; i >= 0; i--) {
    const code = w.charCodeAt(i);
    const cid = code < 512 ? _CID[code] : 0;
    if (cid === 0) break;
    const nxt = _NEXT[(state << 6) | cid];
    if (nxt <= 0) {
      if (nxt < 0) {
        const s = ~nxt;
        if (_NEED[s] <= wLen) bestOp = _OP[s];
      }
      break;
    }
    state = nxt;
    if (_NEED[state] <= wLen) bestOp = _OP[state];
  }

  // 4. Stage 3.5: Apply (cut, add) + _OP_BR-guarded Stem-Bridge Lookup
  if (bestOp >= 0) {
    const cut = _OP_CUT[bestOp];
    const add = _OP_ADD[bestOp];
    if (cut > 0 || add.length > 0) {
      w = (cut > 0 ? w.slice(0, wLen - cut) : w) + add;
      if (_OP_BR[bestOp] === 1) {
        const excTrie = _EXC[w];
        if (excTrie !== undefined) {
          if (_CACHE.size >= _CACHE_MAX) _resetCache();
          _CACHE.set(word, excTrie);
          return excTrie;
        }
      }
    }
  }

  if (_CACHE.size >= _CACHE_MAX) _resetCache();
  _CACHE.set(word, w);
  return w;
}
```


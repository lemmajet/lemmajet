# Python Emitter (`py_emitter.py`) Architecture & Optimizations

This document explains how LemmaJet's optimized Python emitter implements the [Universal 4-Stage Inference Pipeline](model-inference-and-bridge-architecture.md) and why every optimization exists.

> **Distribution Notice**:
> This document is bundled directly into the standalone Python package distribution (`pkg/py/docs/python-emitter-optimizations.md`) to provide Python search engineers and downstream library consumers with full visibility into the bytecode and algorithmic techniques powering LemmaJet in CPython.

---

## 1. Why the Baseline Python Code Was Slow

In the baseline `py_emitter.py`, the inner suffix-trie loop looked like this:

```python
idx = w_len - 1
while idx >= 0:
    if type(curr) is not dict:
        break
    nxt = curr.get(w[idx])
    if nxt is None:
        break
    if type(nxt) is tuple and type(nxt[0]) is str:
        rem, nxt = nxt
        r_len = len(rem)
        if idx < r_len or w[idx - r_len : idx] != rem:
            break
        idx -= r_len
    idx -= 1
    curr = nxt
    o = curr.get("") if type(curr) is dict else curr
    if o is not None:
        if type(o) is int:
            if w_len - o >= _MSL:
                best = o
        elif w_len - o[0] >= 1 and w_len - o[0] + len(o[1]) >= _MSL:
            best = o
```

### CPython Bottlenecks Identified by `cProfile` & Stage Decomposition
On EVERY character of EVERY word (~4.88 steps/word in Polish, ~3.18 steps/word in English), the baseline loop executed:
1. **Two `type(curr) is dict` checks** per character.
2. **Two dictionary `.get()` method calls** per character: `curr.get(w[idx])` and `curr.get("")` (even though >60% of internal trie nodes do not have a `""` operation!). Each `.get()` requires attribute resolution (`LOAD_ATTR`) + bound method call (`CALL`).
3. **Radix tuple check (`type(nxt) is tuple and type(nxt[0]) is str`)** and string slice `w[idx - r_len : idx]` inside the hot loop.
4. **Compound `min_stem_len` arithmetic** (`type(o) is int`, `w_len - o[0] + len(o[1]) >= _MSL`) on every matched op.
5. **Unconditional `_PFX_TRIE` loop entry**: In Polish, 100% of words with `len(w) > 4` entered the prefix trie loop, even though 81% of Polish words do not start with `'n'`.

---

## 2. The 7 Core Python Optimizations

### Optimization 1: Compact On-Disk Literals + Load-Time Linker (`_link`)
- **On-disk size (-11% smaller `.py` files)**:
  - Previously, the emitter pre-formatted radix edges `"ni": 3` into nested tuples `"n": ("i", 3)` in the Python source file, inflating file size.
  - Now, the emitter dumps the raw, compact trie dict `_RAW = {"ni": 3, ...}` directly into the `.py` file (`457 KB -> 408 KB` on PL `md-balanced`, `92.6 KB -> 82.8 KB` on EN `md-balanced`).
- **One-time Load-Time Linking (~15–130 ms at `import` time)**:
  - When the module is imported, `_link(_RAW, _MSL, _RAWDAG, {})` walks `_RAW` once and transforms every character transition into a uniform 3-tuple:
    ```python
    node[ch] = (child_dict, need_len, op_tuple)
    ```
  - **What `_link` resolves once at `import` time so `stem(word)` never has to**:
    1. **Expands multi-character radix keys** (`"ni": 3` $\to$ `{"n": ({"i": (_E, 6, (-3, ""))}, 999999, None)}`) into single-character dict hops.
    2. **Resolves DAG references** (`(dag_idx,)` $\to$ shared linked `dict` instance via `memo`).
    3. **Replaces leaf nodes (`int` or `(cut, add)`)** with a shared empty dict `_E = {}` as `child_dict`, while hoisting the leaf's `(need_len, op_tuple)` directly onto the incoming edge!
    4. **Frees raw literals (`del _RAW, _RAWDAG`)** immediately after linking so no duplicate memory is held.

### Optimization 2: Folded `need` Threshold (`nd <= wl`)
- In `_opinfo(raw, msl)`:
  - If a node has no operation (`raw is None`), `need = 999999` (`_N`).
  - If `raw` is a pure integer cut `c`, `need = c + msl`.
  - If `raw` is an alternation `(c, a)`, `need = max(c + 1, c + msl - len(a))`.
- Inside the character loop, checking BOTH whether the node has an operation AND whether that operation satisfies `min_stem_len` collapses into **one integer comparison**:
  ```python
  node, nd, op = node[ch]
  if nd <= wl:
      best = op
  ```
  Notice: When a node has no operation, `nd` is `999999`, so `nd <= wl` is automatically `False`! We never even check `if op is not None:`!

### Optimization 3: Branchless Negative-Index Slicing (`w = w[:nc] + s`)
- In baseline Python code, applying `best` required type and sign branches:
  ```python
  if type(best) is int:
      w = w[:-best]
  else:
      c, a = best
      w = (w[:-c] if c > 0 else w) + a
  ```
- In `_opinfo`, we precompute the negative slice bound:
  ```python
  nc = -c if c else None
  op = (nc, a)  # where `a` is "" for pure truncations
  ```
- Why this is faster in CPython:
  1. In Python, `w[:None]` is a built-in slice equivalent to `w[:]` (returns `w` with incremented refcount, zero copy!). Thus `w[:nc]` works for **both** `c > 0` (`nc = -c`) and `c == 0` (`nc = None`) with **zero `if c > 0:` branch**!
  2. When `s == ""` (pure truncation), CPython's `unicode_concat` C implementation checks `if (PyUnicode_GET_LENGTH(right) == 0) return Py_NewRef(left);`—short-circuiting `w[:nc] + ""` in C in 0.37 ns without any Python `type(best) is int` check!
  3. Therefore, applying any rule (pure truncation OR alternation, `c > 0` OR `c == 0`) is always two lines:
     ```python
     nc, s = best
     w = w[:nc] + s
     ```

### Optimization 4: `ch in node; node[ch]` vs `node.get(ch)` + Prefix First-Char Guard
- Calling `t = node.get(ch)` in a loop looks up the `.get` attribute on `dict` and invokes a C method descriptor on every character.
- Using `if ch not in node: break` followed by `node, nd, op = node[ch]` uses CPython's specialized `CONTAINS_OP` and `BINARY_SUBSCR_DICT` inline bytecode caches, saving ~19 ns/word.
- For Stage 2 (`_PFX_TRIE`), adding `if wl > _AFF_MIN and w[0] in _PFXR:` before the prefix loop skips Stage 2 in a single dict membership check on **81% of Polish words**.

### Optimization 5: 3-Character Suffix Jump Table (`_jget(w[-3:])`) + Leaf Loop Skip
- In English, **59%** of words terminate their trie walk at depth $\le 3$. Even for words that walk deeper (depth 4–7 in Polish), their first 3 suffix steps always traverse the exact same top 3 levels of `_ROOT`.
- At module load time, `_jump(_ROOT, _ROOT_NEED, _ROOT_OP, k=3)` enumerates all length-3 suffix paths in `_ROOT` and builds a flat dictionary `_JUMP_TABLE`:
  ```python
  # Key: 3-char suffix string in forward word order, e.g. "ing" or "ego"
  # Value: 4-tuple (node_at_depth_3_or_None, nd0, op0, fallback_chain)
  ```
  - `nd0, op0`: The deepest operation along those 3 steps (highest priority).
  - `fallback_chain`: Any shallower operations at depth 0, 1, or 2 with a smaller `need` threshold, used only if `nd0 > wl` (rare: only on very short words of length 3–4).
  - `node_at_depth_3_or_None`: The child dict at depth 3, or `None` if depth 3 is a leaf (`{}`)!
- At runtime:
  - One slice `w[-3:]` + one `_jget(w[-3:])` lookup replaces the first **3 iterations** of the `while` loop!
  - If `t is not None` and `node is None` (depth 3 was a leaf node), **we skip the `while i >= 0:` loop completely!**
  - If `t is None` (the word's 3-char suffix does not reach depth 3 in the trie, meaning match depth is $\le 2$), we walk at most 2 characters from `_ROOT`!

### Optimization 6: Mode-Specialized Stem-Bridge Filter
- As proven in [Model Inference Specification, Section 4](model-inference-and-bridge-architecture.md), Stage 3.5 checks `_EXC.get(w)` after applying the trie rule.
- **Why stemmers and lemmatizers differ**:
  - In **stemmers** (`mode == "stem"`), 81–96% of rules have `add == ""`, where `can_bridge` is always `True`. Unpacking a 3-tuple `nc, s, cb = best` and checking `if cb:` adds overhead without skipping enough lookups. So for stemmers, the emitter emits a 2-tuple `op = (nc, s)`.
  - In **lemmatizers** (`mode == "lemma"`), 65–70% of rules have `add != ""` (e.g. `add = "ować"`), and **65% of those `add` strings never appear at the end of any key in `_EXC`**. So for lemmatizers, the emitter emits a 3-tuple `op = (nc, s, cb)` where `cb = any(k.endswith(a) for k in _EXC)` is precomputed at load time, and Stage 3.5 only runs `if cb: e = _exc_get(w)`!

### Optimization 7: Closure-Bound `_factory`, Inlined Batch Loop & Pre-Seeded `_CACHE`
- **Closure Binding (`LOAD_DEREF` vs `LOAD_GLOBAL`)**:
  - Wrapping `stem` and `stem_words` inside `def _factory(_CACHE=_CACHE, _cache_get=_CACHE.get, _ROOT=_ROOT, ...):` binds all lookup tables and methods as local closure arguments instead of module globals.
- **Universal Drop-In API Alignment**:
  - The batch function is exposed as `stem_words(words)` / `lemmatize_words(words)` (with camelCase `stemWords` / `lemmatizeWords` and class methods on `Stemmer` / `Lemmatizer`).
- **Zero-Redundancy Pre-Seeded `_CACHE`**:
  - `_CACHE` (bounded at `131,072` entries) is initialized with `dict(_EXC)` plus `"" -> ""`.
  - At the top of `stem(word)`:
    ```python
    r = _cache_get(word)
    if r is not None:
        return r
    w = word.lower()
    if w != word:
        r = _cache_get(w)
        if r is not None:
            if len(_CACHE) >= _CACHE_MAX:
                _reset_cache()
            _CACHE[word] = r
            return r
    ```
  - **Why this is so fast**:
    1. **Warm hit / Surface exception / Empty string**: Finished in **2 lines** (`r = _cache_get(word); if r is not None: return r`) in ~35 ns—doesn't even call `word.lower()`!
    2. **Cold lowercase miss (`w == word`)**: Since `_CACHE` contains all of `_EXC`, if `word` missed `_CACHE` and `w == word`, `w` is guaranteed not to be in `_EXC`. Thus **Stage 1 `_exc_get(w)` is completely eliminated**!
    3. **Uppercase input (`w != word`, e.g. `"Running"` or `"Went"`)**: Checks `_cache_get(w)` once, which catches both uppercase surface exceptions (`"Went" -> "went" -> "go"`) and already-cached lowercase words (`"Running" -> "running"`), and caches the uppercase form too!

---

## 3. Annotated Walkthrough of Emitted Python Code

```python
# 1. Compact raw constants on disk (deleted after load-time linking)
_EXC = {"went": "go", ...}
_RAW = {"s": 1, "g": {"ni": 3}, ...}
_RAWDAG = ()
_MSL = 3
_AFF_MIN = 4
_RAWPFX = {"n": {"a": {"j": {"": 3, ...}}, ...}}

# 2. Load-time linker & 3-gram jump builder (runs once on import)
_N = 999999
_E = {}
...
_ROOT = _link(_RAW, _MSL, _RAWDAG, {})
_ROOT_NEED, _ROOT_OP = _opinfo(_RAW, _MSL)
_PFXR = _plink(_RAWPFX, _AFF_MIN)
del _RAW, _RAWDAG, _RAWPFX
_jget = _jump(_ROOT, _ROOT_NEED, _ROOT_OP, 3).get

# 3. Bounded cache pre-seeded with _EXC and "" -> ""
_CACHE_MAX = 131072
_CACHE = dict(_EXC)
_CACHE[""] = ""

def _reset_cache():
    _CACHE.clear()
    _CACHE.update(_EXC)
    _CACHE[""] = ""

# 4. Closure factory for fast local/deref variable access
def _factory(_CACHE=_CACHE, _cache_get=_CACHE.get, _CACHE_MAX=_CACHE_MAX,
             _EXC=_EXC, _exc_get=_EXC.get, _ROOT=_ROOT, _ROOT_OP=_ROOT_OP,
             _ROOT_NEED=_ROOT_NEED, _jget=_jget, _AFF_MIN=_AFF_MIN, _PFXR=_PFXR):

    def stem(word):
        # Fast path: hits cached words, surface _EXC (lowercase), and ""
        r = _cache_get(word)
        if r is not None:
            return r
        w = word.lower()
        # Only probe cache/exceptions again if word had uppercase letters
        if w != word:
            r = _cache_get(w)
            if r is not None:
                if len(_CACHE) >= _CACHE_MAX:
                    _reset_cache()
                _CACHE[word] = r
                return r

        # Stage 2: Forward Prefix Trie (guarded by w[0] in _PFXR)
        wl = len(w)
        if wl > _AFF_MIN and w[0] in _PFXR:
            node = _PFXR
            bp = 0
            for ch in w:
                if ch not in node:
                    break
                node, nd, op = node[ch]
                if nd <= wl:
                    bp = op
            if bp:
                w = w[bp:]
                e = _exc_get(w)
                if e is not None:
                    if len(_CACHE) >= _CACHE_MAX:
                        _reset_cache()
                    _CACHE[word] = e
                    return e

        # Stage 3: 3-char suffix jump + conditional remaining trie walk
        wl = len(w)
        t = _jget(w[-3:]) if wl >= 3 else None
        if t is not None:
            node, nd, best, rest = t
            if nd > wl:
                best = None
                for nd, op in rest:
                    if nd <= wl:
                        best = op
                        break
            if node is not None:
                i = wl - 4
                while i >= 0:
                    ch = w[i]
                    if ch not in node:
                        break
                    node, nd, op = node[ch]
                    if nd <= wl:
                        best = op
                    i -= 1
        else:
            best = _ROOT_OP if _ROOT_NEED <= wl else None
            node = _ROOT
            i = wl - 1
            while i >= 0:
                ch = w[i]
                if ch not in node:
                    break
                node, nd, op = node[ch]
                if nd <= wl:
                    best = op
                i -= 1

        # Stage 3.5: Branchless negative slice + post-trie stem-bridge check
        if best is not None:
            nc, s = best          # (or `nc, s, cb = best` in lemmatizer mode)
            w = w[:nc] + s
            e = _exc_get(w)       # (guarded by `if cb:` in lemmatizer mode)
            if e is not None:
                if len(_CACHE) >= _CACHE_MAX:
                    _reset_cache()
                _CACHE[word] = e
                return e

        if len(_CACHE) >= _CACHE_MAX:
            _reset_cache()
        _CACHE[word] = w
        return w

    def stem_words(words):
        # Fully inlined batch loop (no per-word function call overhead)
        ...
```


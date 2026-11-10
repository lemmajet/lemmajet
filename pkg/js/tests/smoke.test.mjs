import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

import {
  MODES,
  SIZES,
  SUPPORTED_LANGUAGES,
  VARIANTS,
} from "../dist/index.js";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const plStemmerDefault = path.resolve(__dirname, "../dist/stemmer/pl.js");
const skipSmoke = !fs.existsSync(plStemmerDefault);

const SAMPLE_WORDS = {
  pl: "psami",
  en: "running",
  de: "häuser",
  es: "casas",
  fr: "chats",
  cs: "městy",
};

const EXPECTED_MD_BALANCED = {
  pl: { stem: ["pies", "ps"], lemma: ["pies"] },
  en: { stem: ["run"], lemma: ["run"] },
  de: { stem: ["haus", "häuser"], lemma: ["haus"] },
  es: { stem: ["cas", "casas"], lemma: ["casa"] },
  fr: { stem: ["chat", "chats"], lemma: ["chat", "chats"] },
  cs: { stem: ["měst"], lemma: ["město"] },
};

test("default language root re-exports (stemmer & lemmatizer)", {
  skip: skipSmoke ? "run scripts/build_pkg.py first" : false,
}, async () => {
  const langNames = {
    pl: "Polish",
    en: "English",
    de: "German",
    es: "Spanish",
    fr: "French",
    cs: "Czech",
  };

  for (const lang of SUPPORTED_LANGUAGES) {
    // Stemmer root
    const sMod = await import(`../dist/stemmer/${lang}.js`);
    assert.equal(typeof sMod.stem, "function");
    assert.equal(typeof sMod.stemWord, "function");
    assert.equal(typeof sMod.stemWords, "function");
    assert.equal(typeof sMod.clearCache, "function");
    assert.equal(typeof sMod.Stemmer, "function");
    assert.equal(typeof sMod.default, "function");
    assert.equal(typeof sMod.stemBatch, "undefined");

    // Snowball language-named class alias (e.g. EnglishStemmer)
    const langStemmerName = `${langNames[lang]}Stemmer`;
    assert.equal(typeof sMod[langStemmerName], "function");
    assert.equal(sMod[langStemmerName], sMod.Stemmer);

    const word = SAMPLE_WORDS[lang];
    const sRes = sMod.stem(word);
    assert.ok(typeof sRes === "string" && sRes.length > 0);
    assert.ok(
      EXPECTED_MD_BALANCED[lang].stem.includes(sRes),
      `Expected stem of ${word} to be one of ${EXPECTED_MD_BALANCED[lang].stem}, got ${sRes}`
    );
    assert.deepEqual(sMod.stemWords([word, word]), [sRes, sRes]);
    assert.equal(sMod.stemWord(word), sRes);

    // Static Stemmer methods
    assert.equal(sMod.Stemmer.stem(word), sRes);
    assert.equal(sMod.Stemmer.stemWord(word), sRes);
    assert.deepEqual(sMod.Stemmer.stemWords([word, word]), [sRes, sRes]);

    // Instance Stemmer methods
    const s = new sMod.Stemmer();
    assert.equal(s.stem(word), sRes);
    assert.equal(s.stemWord(word), sRes);
    assert.deepEqual(s.stemWords([word]), [sRes]);
    s.clearCache();

    // Instance via language-named class
    const ls = new sMod[langStemmerName]();
    assert.equal(ls.stemWord(word), sRes);
    assert.deepEqual(ls.stemWords([word, word]), [sRes, sRes]);

    // Lemmatizer root
    const lMod = await import(`../dist/lemmatizer/${lang}.js`);
    assert.equal(typeof lMod.lemmatize, "function");
    assert.equal(typeof lMod.lemmatizeWord, "function");
    assert.equal(typeof lMod.lemmatizeWords, "function");
    assert.equal(typeof lMod.clearCache, "function");
    assert.equal(typeof lMod.Lemmatizer, "function");
    assert.equal(typeof lMod.default, "function");
    assert.equal(typeof lMod.lemmatizeBatch, "undefined");

    // Snowball language-named class aliases (e.g. EnglishLemmatizer, EnglishStemmer)
    const langLemmatizerName = `${langNames[lang]}Lemmatizer`;
    assert.equal(typeof lMod[langLemmatizerName], "function");
    assert.equal(lMod[langLemmatizerName], lMod.Lemmatizer);
    assert.equal(typeof lMod[langStemmerName], "function");
    assert.equal(lMod[langStemmerName], lMod.Lemmatizer);

    // Snowball drop-in aliases (D-14)
    assert.equal(lMod.stem, lMod.lemmatize);
    assert.equal(lMod.stemWord, lMod.lemmatizeWord);
    assert.equal(lMod.stemWords, lMod.lemmatizeWords);
    assert.equal(lMod.Stemmer, lMod.Lemmatizer);

    const lRes = lMod.lemmatize(word);
    assert.ok(typeof lRes === "string" && lRes.length > 0);
    assert.ok(
      EXPECTED_MD_BALANCED[lang].lemma.includes(lRes),
      `Expected lemma of ${word} to be one of ${EXPECTED_MD_BALANCED[lang].lemma}, got ${lRes}`
    );
    assert.deepEqual(lMod.lemmatizeWords([word, word]), [lRes, lRes]);
    assert.equal(lMod.lemmatizeWord(word), lRes);
    assert.deepEqual(lMod.stemWords([word, word]), [lRes, lRes]);
    assert.equal(lMod.stemWord(word), lRes);

    // Static Lemmatizer methods
    assert.equal(lMod.Lemmatizer.lemmatize(word), lRes);
    assert.equal(lMod.Lemmatizer.lemmatizeWord(word), lRes);
    assert.deepEqual(lMod.Lemmatizer.lemmatizeWords([word, word]), [lRes, lRes]);
    assert.equal(lMod.Lemmatizer.stem(word), lRes);
    assert.equal(lMod.Lemmatizer.stemWord(word), lRes);
    assert.deepEqual(lMod.Lemmatizer.stemWords([word, word]), [lRes, lRes]);

    // Instance Lemmatizer methods
    const l = new lMod.Lemmatizer();
    assert.equal(l.lemmatize(word), lRes);
    assert.equal(l.lemmatizeWord(word), lRes);
    assert.deepEqual(l.lemmatizeWords([word]), [lRes]);
    assert.equal(l.stem(word), lRes);
    assert.equal(l.stemWord(word), lRes);
    assert.deepEqual(l.stemWords([word]), [lRes]);
    l.clearCache();

    // Instance via language-named aliases
    const ll = new lMod[langLemmatizerName]();
    assert.equal(ll.lemmatizeWord(word), lRes);
    assert.equal(ll.stemWord(word), lRes);
  }
});

test("all 216 profile ESM modules", {
  skip: skipSmoke ? "run scripts/build_pkg.py first" : false,
}, async () => {
  for (const lang of SUPPORTED_LANGUAGES) {
    const word = SAMPLE_WORDS[lang];
    for (const mode of MODES) {
      const fnName = mode === "stemmer" ? "stem" : "lemmatize";
      const batchFnName = mode === "stemmer" ? "stemWords" : "lemmatizeWords";
      const legacyBatchFnName = mode === "stemmer" ? "stemBatch" : "lemmatizeBatch";

      for (const size of SIZES) {
        for (const variant of VARIANTS) {
          const modPath = `../dist/${mode}/${lang}/${size}_${variant}.js`;
          const mod = await import(modPath);

          const fn = mod[fnName];
          const batchFn = mod[batchFnName];
          assert.equal(typeof fn, "function", `Missing ${fnName} in ${modPath}`);
          assert.equal(typeof batchFn, "function", `Missing ${batchFnName} in ${modPath}`);
          assert.equal(typeof mod[legacyBatchFnName], "undefined", `Legacy ${legacyBatchFnName} must not exist in ${modPath}`);

          const res = fn(word);
          assert.ok(
            typeof res === "string" && res.length > 0,
            `Expected non-empty string for ${word} in ${modPath}, got ${res}`
          );
          assert.deepEqual(batchFn([word]), [res]);

          // Snowball drop-in API checks across all 216 modules
          assert.equal(typeof mod.stemWord, "function", `Missing stemWord in ${modPath}`);
          assert.equal(typeof mod.stemWords, "function", `Missing stemWords in ${modPath}`);
          assert.equal(mod.stemWord(word), res);
          assert.deepEqual(mod.stemWords([word]), [res]);

          const Cls = mod.Stemmer || mod.Lemmatizer || mod.default;
          assert.equal(typeof Cls, "function", `Missing class in ${modPath}`);
          assert.equal(Cls.stemWord(word), res);
          assert.deepEqual(Cls.stemWords([word]), [res]);
          const inst = new Cls();
          assert.equal(inst.stemWord(word), res);
          assert.deepEqual(inst.stemWords([word]), [res]);
        }
      }
    }
  }
});


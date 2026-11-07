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
  for (const lang of SUPPORTED_LANGUAGES) {
    // Stemmer root
    const sMod = await import(`../dist/stemmer/${lang}.js`);
    assert.equal(typeof sMod.stem, "function");
    assert.equal(typeof sMod.stemBatch, "function");
    assert.equal(typeof sMod.default, "function");

    const word = SAMPLE_WORDS[lang];
    const sRes = sMod.stem(word);
    assert.ok(typeof sRes === "string" && sRes.length > 0);
    assert.ok(
      EXPECTED_MD_BALANCED[lang].stem.includes(sRes),
      `Expected stem of ${word} to be one of ${EXPECTED_MD_BALANCED[lang].stem}, got ${sRes}`
    );
    assert.deepEqual(sMod.stemBatch([word, word]), [sRes, sRes]);

    // Lemmatizer root
    const lMod = await import(`../dist/lemmatizer/${lang}.js`);
    assert.equal(typeof lMod.lemmatize, "function");
    assert.equal(typeof lMod.lemmatizeBatch, "function");
    assert.equal(typeof lMod.default, "function");
    assert.equal(typeof lMod.stem, "undefined");

    const lRes = lMod.lemmatize(word);
    assert.ok(typeof lRes === "string" && lRes.length > 0);
    assert.ok(
      EXPECTED_MD_BALANCED[lang].lemma.includes(lRes),
      `Expected lemma of ${word} to be one of ${EXPECTED_MD_BALANCED[lang].lemma}, got ${lRes}`
    );
    assert.deepEqual(lMod.lemmatizeBatch([word, word]), [lRes, lRes]);
  }
});

test("all 216 profile ESM modules", {
  skip: skipSmoke ? "run scripts/build_pkg.py first" : false,
}, async () => {
  for (const lang of SUPPORTED_LANGUAGES) {
    const word = SAMPLE_WORDS[lang];
    for (const mode of MODES) {
      const fnName = mode === "stemmer" ? "stem" : "lemmatize";
      const batchFnName = mode === "stemmer" ? "stemBatch" : "lemmatizeBatch";

      for (const size of SIZES) {
        for (const variant of VARIANTS) {
          const modPath = `../dist/${mode}/${lang}/${size}_${variant}.js`;
          const mod = await import(modPath);

          const fn = mod[fnName];
          const batchFn = mod[batchFnName];
          assert.equal(typeof fn, "function", `Missing ${fnName} in ${modPath}`);
          assert.equal(typeof batchFn, "function", `Missing ${batchFnName} in ${modPath}`);

          const res = fn(word);
          assert.ok(
            typeof res === "string" && res.length > 0,
            `Expected non-empty string for ${word} in ${modPath}, got ${res}`
          );
          assert.deepEqual(batchFn([word]), [res]);
        }
      }
    }
  }
});


import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

import defaultExport, {
  DEFAULT_PROFILE,
  DEFAULT_SIZE,
  DEFAULT_VARIANT,
  MODES,
  SIZES,
  SUPPORTED_LANGUAGES,
  VARIANTS,
  VERSION,
} from "../dist/index.js";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

test("metadata constants exports", () => {
  assert.equal(VERSION, "0.6.0");
  assert.deepEqual(SUPPORTED_LANGUAGES, ["pl", "en", "de", "es", "fr", "cs"]);
  assert.deepEqual(SIZES, ["xxs", "xs", "sm", "md", "lg", "xl"]);
  assert.deepEqual(VARIANTS, ["precision", "balanced", "recall"]);
  assert.deepEqual(MODES, ["stemmer", "lemmatizer"]);
  assert.equal(DEFAULT_SIZE, "md");
  assert.equal(DEFAULT_VARIANT, "balanced");
  assert.equal(DEFAULT_PROFILE, "md_balanced");

  assert.equal(defaultExport.VERSION, "0.6.0");
  assert.deepEqual(defaultExport.SUPPORTED_LANGUAGES, SUPPORTED_LANGUAGES);
  assert.deepEqual(defaultExport.SIZES, SIZES);
  assert.deepEqual(defaultExport.VARIANTS, VARIANTS);
  assert.deepEqual(defaultExport.MODES, MODES);
  assert.equal(defaultExport.DEFAULT_SIZE, "md");
  assert.equal(defaultExport.DEFAULT_VARIANT, "balanced");
  assert.equal(defaultExport.DEFAULT_PROFILE, "md_balanced");
});

test("package.json export manifest compliance", () => {
  const pkgPath = path.resolve(__dirname, "../package.json");
  const pkg = JSON.parse(fs.readFileSync(pkgPath, "utf-8"));

  assert.equal(pkg.name, "lemmajet");
  assert.equal(pkg.version, "0.6.0");
  assert.equal(pkg.type, "module");
  assert.equal(pkg.sideEffects, false);
  assert.deepEqual(pkg.dependencies, {});

  // Subpath exports validation
  assert.ok(pkg.exports, "package.json missing exports block");
  assert.deepEqual(pkg.exports["."], {
    types: "./dist/index.d.ts",
    default: "./dist/index.js",
  });
  assert.deepEqual(pkg.exports["./stemmer/*"], {
    types: "./dist/stemmer/*.d.ts",
    default: "./dist/stemmer/*.js",
  });
  assert.deepEqual(pkg.exports["./lemmatizer/*"], {
    types: "./dist/lemmatizer/*.d.ts",
    default: "./dist/lemmatizer/*.js",
  });
});


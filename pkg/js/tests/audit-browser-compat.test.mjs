import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const NODE_BUILTINS = new Set([
  "fs",
  "path",
  "crypto",
  "os",
  "util",
  "events",
  "stream",
  "child_process",
  "http",
  "https",
  "net",
  "tls",
  "url",
  "worker_threads",
  "buffer",
  "process",
]);

const FORBIDDEN_GLOBALS =
  /(?<![\.\w\$\x22\x27`])(process|Buffer|__dirname|__filename|global)(?![\w\$\x22\x27`:])/;
const IMPORT_RE = /import\s+.*?\s+from\s+[\x22\x27]([^\x22\x27]+)[\x22\x27]/g;
const DYNAMIC_IMPORT_RE = /import\s*\(\s*[\x22\x27]([^\x22\x27]+)[\x22\x27]\)/g;
const REQUIRE_RE = /require\s*\(\s*[\x22\x27]([^\x22\x27]+)[\x22\x27]\)/g;

test("verify zero Node.js built-ins or globals in pkg/js/dist", () => {
  const distDir = path.resolve(__dirname, "../dist");
  if (!fs.existsSync(distDir)) {
    return;
  }
  const violations = [];
  let scannedCount = 0;

  function scan(dir) {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        scan(full);
      } else if (entry.name.endsWith(".js") || entry.name.endsWith(".d.ts")) {
        scannedCount++;
        const content = fs.readFileSync(full, "utf-8");

        // Check static imports
        for (const match of content.matchAll(IMPORT_RE)) {
          const spec = match[1];
          if (spec.startsWith("node:") || NODE_BUILTINS.has(spec.split("/")[0])) {
            violations.push({ file: full, type: "import", spec });
          }
        }

        // Check dynamic imports
        for (const match of content.matchAll(DYNAMIC_IMPORT_RE)) {
          const spec = match[1];
          if (spec.startsWith("node:") || NODE_BUILTINS.has(spec.split("/")[0])) {
            violations.push({ file: full, type: "dynamic_import", spec });
          }
        }

        // Check require calls
        for (const match of content.matchAll(REQUIRE_RE)) {
          const spec = match[1];
          if (spec.startsWith("node:") || NODE_BUILTINS.has(spec.split("/")[0])) {
            violations.push({ file: full, type: "require", spec });
          }
        }

        // Strip string literals and comments before checking globals
        const noStrings = content.replace(/"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|`(?:\\.|[^`\\])*`/g, '""');
        const noComments = noStrings.replace(/\/\*[\s\S]*?\*\/|\/\/.*/g, "");
        const globalMatch = noComments.match(FORBIDDEN_GLOBALS);
        if (globalMatch) {
          violations.push({ file: full, type: "global", token: globalMatch[1] });
        }
      }
    }
  }

  scan(distDir);
  assert.equal(
    violations.length,
    0,
    `Found Node built-in/global violations in ${scannedCount} files: ${JSON.stringify(violations, null, 2)}`
  );
  assert.ok(scannedCount > 0, "Expected at least 1 file scanned in pkg/js/dist");
});


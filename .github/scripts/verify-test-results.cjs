'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');

function readText(path) {
  const data = fs.readFileSync(path);
  const encoding = data[0] === 0xff && data[1] === 0xfe ? 'utf16le' : 'utf8';
  return data.toString(encoding).replace(/^\uFEFF/, '')
      .replace(/\x1b\[[0-?]*[ -/]*[@-~]/g, '');
}

function selectStep(path, step) {
  const text = readText(path);
  if (step === 'raw') return text;
  const lines = text.split(/\r?\n/).filter((line) => {
    return line.split('\t')[1] === step;
  });
  assert(lines.length > 0, `Missing workflow step: ${step}`);
  return lines.join('\n');
}

function completedResults(text) {
  const summaries = [...text.matchAll(/Executed \d+ out of (\d+) tests:/g)];
  assert(summaries.length > 0, 'Missing complete Bazel test summary');
  const expected = Number(summaries.at(-1)[1]);
  const results = new Map();
  const pattern = /(\/\/\S+)\s+(?:\(cached\)\s+)?(PASSED|FAILED|TIMEOUT|FLAKY)\b/g;
  for (const match of text.matchAll(pattern)) {
    results.set(match[1], match[2]);
  }
  assert.equal(results.size, expected, 'Incomplete target-result coverage');
  return results;
}

function main(args) {
  assert(args.length === 5 || args.length === 7,
      'Usage: verifier baseline step retry step expected-count [partial step]');
  const baseline = completedResults(selectStep(args[0], args[1]));
  assert.equal(baseline.size, Number(args[4]), 'Unexpected test inventory');
  const initiallyFailed = [...baseline].filter(([, status]) => status !== 'PASSED');
  assert(initiallyFailed.length > 0, 'Expected a failed-target retry');
  const retries = completedResults(selectStep(args[2], args[3]));
  for (const [label, status] of retries) {
    assert(baseline.has(label), `Retry target absent from full suite: ${label}`);
    assert.equal(status, 'PASSED', `Retry did not pass: ${label}`);
    baseline.set(label, status);
  }
  if (args.length === 7) {
    const partial = selectStep(args[5], args[6]);
    assert(!/\bFAIL:\s+\/\//.test(partial),
        'Additional failed target reported in the interrupted attempt');
    assert(!/\bERROR:/.test(partial),
        'Additional build error reported in the interrupted attempt');
  }
  const unresolved = [...baseline].filter(([, status]) => status !== 'PASSED');
  assert.equal(unresolved.length, 0,
      `Unresolved test failures: ${JSON.stringify(unresolved)}`);
  console.log(JSON.stringify({
    totalTargets: baseline.size,
    originalPassed: baseline.size - initiallyFailed.length,
    resolvedFailures: initiallyFailed.map(([label]) => label),
    retryTargets: [...retries.keys()],
    result: 'Every target has a passing result for the unchanged source commit',
  }, null, 2));
}

main(process.argv.slice(2));

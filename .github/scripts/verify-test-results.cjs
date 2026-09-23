'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');

function readText(path) {
  const data = fs.readFileSync(path);
  const encoding = data[0] === 0xff && data[1] === 0xfe ? 'utf16le' : 'utf8';
  return data.toString(encoding).replace(/^\uFEFF/, '')
      .replace(/\x1b\[[0-?]*[ -/]*[@-~]/g, '');
}

function selectStep(path, metadataPath, step) {
  const text = readText(path);
  const metadata = JSON.parse(readText(metadataPath));
  const selected = metadata.steps.filter((entry) => entry.name === step);
  assert.equal(selected.length, 1, `Missing or duplicate workflow step: ${step}`);
  assert.equal(selected[0].status, 'completed', `Unfinished workflow step: ${step}`);
  const start = Date.parse(selected[0].started_at);
  const end = Date.parse(selected[0].completed_at) + 1000;
  assert(Number.isFinite(start) && Number.isFinite(end), 'Missing step times');
  const lines = text.split(/\r?\n/).filter((line) => {
    const timestamp = line.match(/\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}/);
    if (!timestamp) return false;
    const time = Date.parse(`${timestamp[0]}Z`);
    return time >= start && time < end;
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
  assert(args.length === 7 || args.length === 8,
      'Usage: verifier base-log base-meta step retry-log retry-meta step count [partial-step]');
  const baseline = completedResults(selectStep(args[0], args[1], args[2]));
  assert.equal(baseline.size, Number(args[6]), 'Unexpected test inventory');
  const initiallyFailed = [...baseline].filter(([, status]) => status !== 'PASSED');
  assert(initiallyFailed.length > 0, 'Expected a failed-target retry');
  const retryMetadata = JSON.parse(readText(args[4]));
  const retryStep = retryMetadata.steps.find((entry) => entry.name === args[5]);
  assert.equal(retryStep?.conclusion, 'success', 'Retry step did not succeed');
  const retries = completedResults(selectStep(args[3], args[4], args[5]));
  for (const [label, status] of retries) {
    assert(baseline.has(label), `Retry target absent from full suite: ${label}`);
    assert.equal(status, 'PASSED', `Retry did not pass: ${label}`);
    baseline.set(label, status);
  }
  if (args.length === 8) {
    const partial = selectStep(args[3], args[4], args[7]);
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

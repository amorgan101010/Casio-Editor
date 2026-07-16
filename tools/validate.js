// Minimal JSON Schema (draft 2020-12 subset) validator for xwp1.json.
const fs = require('fs');
const base = '/home/aileen/Repositories/Casio-Editor/params/';
const schema = JSON.parse(fs.readFileSync(base + 'xwp1.schema.json'));
const data = JSON.parse(fs.readFileSync(base + 'xwp1.json'));
const errors = [];

function resolveRef(ref) {
  if (!ref.startsWith('#/')) throw new Error('unsupported ref ' + ref);
  let node = schema;
  for (const part of ref.slice(2).split('/')) node = node[part];
  return node;
}
function typeOf(v) {
  if (v === null) return 'null';
  if (Array.isArray(v)) return 'array';
  if (Number.isInteger(v)) return 'integer';
  if (typeof v === 'number') return 'number';
  return typeof v; // string, boolean, object
}
function typeMatch(t, v) {
  const a = typeOf(v);
  if (t === 'number') return a === 'number' || a === 'integer';
  return a === t;
}

function validate(s, v, path) {
  if (s.$ref) { validate(resolveRef(s.$ref), v, path); return; }
  if (s.const !== undefined && v !== s.const) errors.push(`${path}: const mismatch (want ${s.const})`);
  if (s.type) {
    const types = Array.isArray(s.type) ? s.type : [s.type];
    if (!types.some(t => typeMatch(t, v))) { errors.push(`${path}: type ${typeOf(v)} not in [${types}]`); return; }
  }
  if (s.enum && !s.enum.includes(v)) errors.push(`${path}: value ${JSON.stringify(v)} not in enum`);
  if (typeof v === 'string' && s.pattern && !new RegExp(s.pattern).test(v)) errors.push(`${path}: '${v}' fails pattern ${s.pattern}`);
  if (typeof v === 'number') {
    if (s.minimum !== undefined && v < s.minimum) errors.push(`${path}: ${v} < min ${s.minimum}`);
    if (s.maximum !== undefined && v > s.maximum) errors.push(`${path}: ${v} > max ${s.maximum}`);
  }
  if (Array.isArray(v)) {
    if (s.minItems !== undefined && v.length < s.minItems) errors.push(`${path}: too few items`);
    if (s.maxItems !== undefined && v.length > s.maxItems) errors.push(`${path}: too many items`);
    if (s.items) v.forEach((el, i) => validate(s.items, el, `${path}[${i}]`));
  }
  if (typeOf(v) === 'object') {
    if (s.required) for (const r of s.required) if (!(r in v)) errors.push(`${path}: missing required '${r}'`);
    const props = s.properties || {};
    for (const k of Object.keys(v)) {
      if (props[k]) validate(props[k], v[k], `${path}.${k}`);
      else if (s.additionalProperties === false) errors.push(`${path}: unexpected property '${k}'`);
      else if (s.additionalProperties && typeof s.additionalProperties === 'object') validate(s.additionalProperties, v[k], `${path}.${k}`);
    }
  }
}

validate(schema, data, '$');
if (errors.length === 0) {
  console.log('VALID: xwp1.json conforms to xwp1.schema.json');
} else {
  console.log('INVALID -', errors.length, 'error(s):');
  errors.slice(0, 40).forEach(e => console.log('  ' + e));
  process.exit(1);
}

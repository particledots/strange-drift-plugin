// Runs the real Scripter mapper script with a stand-in for Logic's Scripter, fed by the engine's real CC stream
// (from strangedrift_ccdump). Also chains it with a copy of the Ogdoad Mapper (tests/fixtures), in both orders, to show
// the two plugins can share a track.
// Usage: STRANGEDRIFT_CCDUMP=<path to strangedrift_ccdump> node mapper_test.js <StrangeDrift-Mapper.js> <Ogdoad-Mapper.js>

"use strict";
const fs = require("fs");
const vm = require("vm");
const { execFileSync } = require("child_process");

let checks = 0, failures = 0;
function expect(ok, what) { checks++; if (!ok) { failures++; console.log("FAIL: " + what); } }

const scriptText = fs.readFileSync(process.argv[2], "utf8");
const ogdoadText = fs.readFileSync(process.argv[3], "utf8");
const dumpExe = process.env.STRANGEDRIFT_CCDUMP;

function readDump(seconds, speed) {
    const lines = execFileSync(dumpExe, [String(seconds), String(speed)], { maxBuffer: 1 << 28 }).toString().trim().split("\n");
    const d = { events: [], finals: [] };
    for (const line of lines) {
        const p = line.split(" ");
        if (p[0] === "final") d.finals[+p[1]] = +p[2];
        else d.events.push({ sample: +p[0], cc: +p[1], value: +p[2] });
    }
    return d;
}
const dump = readDump(120, 64);
const events = dump.events, finals = dump.finals;
expect(events.length > 5000, "the engine produced a CC stream (" + events.length + " messages)");
const isOurCc = n => n >= 106 && n <= 119;
expect(events.every(e => isOurCc(e.cc)), "the stream uses only CC 106-119");
expect(finals.length === 7 && finals.every(v => v >= 0 && v <= 16383), "seven final values");

// ---- A stand-in for Scripter ---------------------------------------------------------------------------------
function makeScripter(text, settings, callParameterChanged, passTo) {
    const out = { passed: [], targets: [] };
    class Event { send() { out.passed.push(this); if (passTo) passTo(this); } trace() {} }
    class ControlChange extends Event { constructor(o) { super(); Object.assign(this, { channel: 1, number: 0, value: 0 }, o); } }
    class NoteOn extends Event { constructor(o) { super(); Object.assign(this, { channel: 1, pitch: 60, velocity: 100 }, o); } }
    class NoteOff extends Event { constructor(o) { super(); Object.assign(this, { channel: 1, pitch: 60, velocity: 0 }, o); } }
    class TargetEvent { constructor() { this.target = ""; this.value = 0; } send() { out.targets.push({ target: this.target, value: this.value }); } }
    const values = {};
    const sandbox = {
        Event, ControlChange, NoteOn, NoteOff, TargetEvent, Math,
        GetParameter: (name) => { if (!(name in values)) throw new Error("GetParameter unknown name " + name); return values[name]; },
        Trace: () => {},
    };
    vm.createContext(sandbox);
    vm.runInContext(text, sandbox);
    const params = vm.runInContext("PluginParameters", sandbox);
    params.forEach(p => { if (p.type !== "target") values[p.name] = p.defaultValue !== undefined ? p.defaultValue : 0; });
    for (const [name, v] of Object.entries(settings)) {
        if (!(name in values)) throw new Error("test uses unknown setting " + name);
        values[name] = v;
        if (callParameterChanged) sandbox.ParameterChanged(params.findIndex(p => p.name === name), v);
    }
    return { sandbox, params, out, values };
}
const near = (a, b) => Math.abs(a - b) < 1e-9;

// ---- 1. The script itself -----------------------------------------------------------------------------------------
{
    const { params } = makeScripter(scriptText, {}, false);
    const names = params.map(p => p.name);
    expect(params.length === 33, "33 settings (channel + 8 lanes x 4)");
    expect(new Set(names).size === names.length, "setting names are unique");
    expect(params.filter(p => p.type === "target").length === 8, "8 Learn targets");
    expect(params.every(p => ["lin", "menu", "target"].includes(p.type)), "only setting types Scripter knows");
    expect(params[0].name === "Strange Drift MIDI channel" && params[0].defaultValue === 15, "channel setting, 15 by default");
    const src = params.find(p => p.name === "Lane 1 source").valueStrings;
    expect(src.join() === "Off,Ripple X,Ripple Y,Ripple Z,Tide X,Tide Y,Tide Z,Glacier X,Glacier Y,Glacier Z,Blend +,Blend -,Blend +/-",
        "Source menu in its fixed order");
    expect(src.every(x => /^[ -~]+$/.test(x)) && /^[\x09\x0a\x0d -~]*$/.test(scriptText), "plain characters only (safe to paste)");
    expect(!/\.trace\(\)/.test(scriptText), "no trace printing in the mapper");
}

// ---- 2. Nothing set up: our CC stops, everything else passes (Ogdoad's CC included) ---------------------------------
{
    const s = makeScripter(scriptText, {}, false);
    const { ControlChange, NoteOn, NoteOff } = s.sandbox;
    for (const e of events.slice(0, 2000)) s.sandbox.HandleMIDI(new ControlChange({ channel: 15, number: e.cc, value: e.value }));
    s.sandbox.HandleMIDI(new NoteOn({ pitch: 64 }));
    s.sandbox.HandleMIDI(new NoteOff({ pitch: 64 }));
    s.sandbox.HandleMIDI(new ControlChange({ channel: 1, number: 1, value: 90 }));
    s.sandbox.HandleMIDI(new ControlChange({ channel: 15, number: 7, value: 100 }));
    let ogdoad = 0;
    for (let n = 12; n <= 31; n++) { s.sandbox.HandleMIDI(new ControlChange({ channel: 16, number: n, value: 3 })); ogdoad++; }
    for (let n = 44; n <= 63; n++) { s.sandbox.HandleMIDI(new ControlChange({ channel: 16, number: n, value: 5 })); ogdoad++; }
    for (let n = 102; n <= 119; n++) { s.sandbox.HandleMIDI(new ControlChange({ channel: 16, number: n, value: 127 })); ogdoad++; }
    expect(s.out.targets.length === 0, "lanes Off: no knob moves");
    expect(s.out.passed.length === 4 + ogdoad, "notes, other CC and all of channel 16 pass through (" + s.out.passed.length + ")");
    expect(s.out.passed[0].pitch === 64 && s.out.passed[0] instanceof NoteOn, "notes pass unchanged");
    expect(s.out.passed.every(e => !(e.channel === 15 && isOurCc(e.number))), "Strange Drift's own CC never reaches the instrument");
    const s2 = makeScripter(scriptText, {}, false);
    for (const e of events) s2.sandbox.HandleMIDI(new s2.sandbox.ControlChange({ channel: 15, number: e.cc, value: e.value }));
    expect(s2.out.passed.length === 0, "the whole 120 s stream stops at the mapper");
}

// ---- 3. All lanes set up, real stream ---------------------------------------------------------------------------------
for (const viaParameterChanged of [true, false]) {
    const how = viaParameterChanged ? " (settings changed in the panel)" : " (settings restored with a project)";
    const settings = {
        "Lane 1 source": 1,                                           // Ripple X
        "Lane 2 source": 2, "Lane 2 min %": 20, "Lane 2 max %": 80,   // Ripple Y, 20..80%
        "Lane 3 source": 3,                                           // Ripple Z
        "Lane 4 source": 8, "Lane 4 min %": 100, "Lane 4 max %": 0,   // Glacier Y upside down
        "Lane 5 source": 6,                                           // Tide Z
        "Lane 6 source": 10,                                          // Blend +
        "Lane 7 source": 11, "Lane 7 min %": 10, "Lane 7 max %": 60,  // Blend -
        "Lane 8 source": 12,                                          // Blend +/-
    };
    const s = makeScripter(scriptText, settings, viaParameterChanged);
    const { ControlChange, NoteOn } = s.sandbox;
    let notesIn = 0;
    const t0 = process.hrtime.bigint();
    events.forEach((e, k) => {
        s.sandbox.HandleMIDI(new ControlChange({ channel: 15, number: e.cc, value: e.value }));
        if (k % 500 === 0) { s.sandbox.HandleMIDI(new NoteOn({ pitch: 40 + (k % 30) })); notesIn++; }
    });
    const usPerEvent = Number(process.hrtime.bigint() - t0) / 1000 / events.length;
    const lane = n => s.out.targets.filter(t => t.target === "Lane " + n + " target").map(t => t.value);
    const last = a => a[a.length - 1];
    const L = [null, lane(1), lane(2), lane(3), lane(4), lane(5), lane(6), lane(7), lane(8)];
    expect(L.slice(1).every(a => a.length > 20), "every lane moves its knob" + how);
    expect(near(last(L[1]), finals[0] / 16383), "Ripple X arrives exactly" + how);
    expect(near(last(L[2]), 0.2 + 0.6 * finals[1] / 16383) && L[2].every(v => v >= 0.2 - 1e-12 && v <= 0.8 + 1e-12), "Ripple Y scaled to 20..80%" + how);
    expect(near(last(L[3]), 1 - finals[1] / 16383), "Ripple Z is Ripple Y upside down" + how);
    expect(near(last(L[4]), 1 - finals[5] / 16383), "Glacier Y with max below min turns upside down" + how);
    expect(near(last(L[5]), 1 - finals[3] / 16383), "Tide Z is Tide Y upside down" + how);
    const b = finals[6];
    const plus = b > 8192 ? (b - 8192) / 8191 : 0, minus = b < 8192 ? (8192 - b) / 8192 : 0;
    expect(near(last(L[6]), plus) && near(last(L[7]), 0.1 + 0.5 * minus) && near(last(L[8]), b / 16383), "Blend +, Blend - and Blend +/- arrive exactly" + how);
    expect(L[6].some(v => v === 0) && L[6].some(v => v > 0.1) && L[7].some(v => near(v, 0.1)) && L[7].some(v => v > 0.15),
        "Blend + and Blend - rest at Min while the blend is on the other side, and rise when it is on theirs" + how);
    expect(L.slice(1).every(a => a.every(v => v >= 0 && v <= 1)), "targets always 0..1" + how);
    const distinct = new Set(L[1].map(v => Math.round(v * 16383))).size;
    expect(distinct > 3000, "14-bit detail reaches the target (" + distinct + " distinct values)" + how);
    const notCoarse = L[1].filter(v => Math.round(v * 16383) % 128 !== 0).length > L[1].length / 2;
    expect(notCoarse, "values arrive in 14-bit steps, not 128 coarse steps" + how);
    expect(Math.max(...L[1]) > 0.85 && Math.min(...L[1]) < 0.15, "Ripple X sweeps most of the range" + how);
    expect(s.out.passed.length === notesIn, "notes still pass with lanes in use" + how);
    console.log("mapper: " + usPerEvent.toFixed(2) + " microseconds per CC message in Node" + how);
}

// ---- 4. A different channel ------------------------------------------------------------------------------------------
{
    const s = makeScripter(scriptText, { "Strange Drift MIDI channel": 14, "Lane 1 source": 1 }, true);
    const { ControlChange } = s.sandbox;
    for (const e of events.slice(0, 400)) s.sandbox.HandleMIDI(new ControlChange({ channel: 15, number: e.cc, value: e.value }));
    expect(s.out.targets.length === 0 && s.out.passed.length === 400, "channel 14 set: channel 15 CC is left alone");
    for (const e of events.slice(0, 400)) s.sandbox.HandleMIDI(new ControlChange({ channel: 14, number: e.cc, value: e.value }));
    expect(s.out.targets.length > 10, "channel 14 set: channel 14 CC is mapped");
}

// ---- 5. Sharing a track with Ogdoad: both mappers chained, in both orders ----------------------------------------------
{
    // A stand-in Ogdoad stream on channel 16: LFO 1 (CC 20 + 52) climbing, Fold 1 (CC 12 + 44), a trigger pulse (CC 102).
    const mixed = [];
    let k = 0;
    for (const e of events) {
        mixed.push({ channel: 15, number: e.cc, value: e.value });
        if (k++ % 3 === 0) {
            const v = (k * 37) % 16384;
            mixed.push({ channel: 16, number: 20, value: v >> 7 }, { channel: 16, number: 52, value: v & 127 });
            mixed.push({ channel: 16, number: 12, value: (v >> 7) ^ 64 }, { channel: 16, number: 44, value: v & 127 });
            if (k % 300 === 1) mixed.push({ channel: 16, number: 102, value: 127 }, { channel: 16, number: 102, value: 0 });
        }
    }
    const lastOgdoad = (() => { for (let i = mixed.length - 1; i >= 0; i--) if (mixed[i].channel === 16 && mixed[i].number === 52) return (mixed[i - 1].value * 128 + mixed[i].value) / 16383; })();
    // An event leaving one Scripter arrives at the next as that Scripter's own kind of event.
    const carry = (to, ev) => new to.sandbox[ev.constructor.name](Object.assign({}, ev));
    for (const sdFirst of [true, false]) {
        const order = sdFirst ? " (Strange Drift Mapper above the Ogdoad Mapper)" : " (Ogdoad Mapper above the Strange Drift Mapper)";
        const instrument = [];
        const secondPass = ev => instrument.push(ev);
        const sdSettings = { "Lane 1 source": 1 }, ogSettings = { "Lane 1 source": 1, "Lane 2 source": 9 };
        let sd, og;
        if (sdFirst) {
            og = makeScripter(ogdoadText, ogSettings, true, secondPass);
            sd = makeScripter(scriptText, sdSettings, true, ev => og.sandbox.HandleMIDI(carry(og, ev)));
        } else {
            sd = makeScripter(scriptText, sdSettings, true, secondPass);
            og = makeScripter(ogdoadText, ogSettings, true, ev => sd.sandbox.HandleMIDI(carry(sd, ev)));
        }
        const first = sdFirst ? sd : og;
        for (const m of mixed) first.sandbox.HandleMIDI(new first.sandbox.ControlChange(m));
        first.sandbox.HandleMIDI(new first.sandbox.NoteOn({ pitch: 50 }));
        const sdL1 = sd.out.targets.filter(t => t.target === "Lane 1 target").map(t => t.value);
        const ogL1 = og.out.targets.filter(t => t.target === "Lane 1 target").map(t => t.value);
        const ogL2 = og.out.targets.filter(t => t.target === "Lane 2 target").map(t => t.value);
        expect(near(sdL1[sdL1.length - 1], finals[0] / 16383) && sdL1.length > 100, "Strange Drift lane ends on Ripple X" + order);
        expect(near(ogL1[ogL1.length - 1], lastOgdoad) && ogL1.length > 100 && ogL2.length > 100, "Ogdoad lanes follow Ogdoad's stream" + order);
        expect(instrument.length === 1 && instrument[0].pitch === 50, "only the note reaches the instrument: both plugins' CC stop at their own mapper" + order);
    }
}

if (failures === 0) console.log("mapper tests: " + checks + " checks passed");
else console.log("mapper tests: " + failures + " of " + checks + " checks FAILED");
process.exit(failures === 0 ? 0 : 1);

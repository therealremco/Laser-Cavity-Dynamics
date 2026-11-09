import { TimePlot, si } from './plot.js';

const form = document.getElementById('controls');
const status = document.getElementById('status');
const output = document.getElementById('output');

// Form fields are in convenient units; the simulation takes SI.
const scale = {
    cavity_length: 1e-3,
    round_trips: 1,
    cross_section: 1e-20,
    internal_loss: 1,
    pulse_energy: 1e-6,
    pulse_duration: 1e-12,
    power: 1,
    initial_inversion: 1e22,
    pump_delay: 1e-12,
    pump_wavelength: 1e-9,
    absorbed_fraction: 1,
    efficiency: 1,
};

const defaults = new FormData(form);

function currentMode() {
    return form.elements.pump_mode.value;
}

function readParameters() {
    const params = { pump_mode: currentMode() };
    for (const [key, factor] of Object.entries(scale)) {
        const field = form.elements[key];
        if (field.value.trim() === '') throw new Error(`${field.closest('label').firstChild.textContent.trim()} is empty`);
        params[key] = Number(field.value) * factor;
    }
    return params;
}

function showModeFields() {
    const mode = currentMode();
    for (const label of form.querySelectorAll('[data-mode]')) {
        label.hidden = !label.dataset.mode.split(' ').includes(mode);
    }
    form.querySelector('[data-delay-label]').textContent =
        mode === 'pulse' ? 'Arrival of the pulse peak' : 'Moment of inversion';
}

// --- Plots -----------------------------------------------------------------

const energyPlot = new TimePlot(document.getElementById('plot-energy'), {
    xLabel: 'Time',
    axes: { left: { unit: 'J' }, right: { unit: 'J' } },
});
const statePlot = new TimePlot(document.getElementById('plot-state'), {
    xLabel: 'Time',
    axes: { left: { unit: '' }, right: { unit: '' } },
});
document.getElementById('log-scale').addEventListener('change', (event) => {
    energyPlot.setLogLeft(event.target.checked);
});

function render(result) {
    const c = { s1: '--series-1', s2: '--series-2', s3: '--series-3' };
    // Energy per time step Δt: the rates from the simulation are step averages,
    // so P·Δt is exactly the energy that passed in that step.
    const perStep = (power) => power.map((p) => p * result.time_step);
    document.getElementById('energy-step').textContent = si(result.time_step, 's');
    energyPlot.setData({
        x: result.time,
        series: [
            { y: perStep(result.pump_power), label: 'Pump energy', unit: 'J', axis: 'right', color: c.s2 },
            { y: perStep(result.output_power), label: 'Output energy', unit: 'J', axis: 'left', color: c.s1 },
        ],
    });
    statePlot.setData({
        x: result.time,
        series: [
            { y: result.total_inversion, label: 'Excited molecules', unit: '', axis: 'left', color: c.s3 },
            { y: result.cavity_photons, label: 'Photons in cavity', unit: '', axis: 'right', color: c.s1 },
        ],
    });

    const pulses = result.pulses;
    document.getElementById('summary').innerHTML = [
        ['Pulses detected', pulses.length],
        ['Total output energy', si(result.total_output_energy, 'J')],
        ['Round-trip time', si(result.round_trip_time, 's')],
        ['Grid', `${result.cells} cells, Δt = ${si(result.time_step, 's')}`],
        ['Cavity length on the grid', si(result.actual_length, 'm', 4)],
        ['Gain medium', `${si(result.gain_start, 'm', 4)} – ${si(result.gain_start + result.fixed.gain_length, 'm', 4)}`],
    ].map(([k, v]) => `<div><dt>${k}</dt><dd>${v}</dd></div>`).join('');

    document.querySelector('#pulses tbody').innerHTML = pulses.length === 0
        ? '<tr><td colspan="5" class="empty">No pulses above threshold</td></tr>'
        : pulses.map((p, i) => `<tr><td>${i + 1}</td><td>${si(p.peak_time, 's', 4)}</td>
            <td>${si(p.energy, 'J')}</td><td>${si(p.fwhm, 's')}</td>
            <td>${i > 0 ? si(p.peak_time - pulses[i - 1].peak_time, 's') : '–'}</td></tr>`).join('');

    const f = result.fixed;
    document.getElementById('fixed').innerHTML = [
        ['High reflector', f.r_hr], ['Output coupler', f.r_oc],
        ['Background index', f.n_background], ['Mode area', si(f.mode_area, 'm²')],
        ['Gain medium length', si(f.gain_length, 'm')], ['Gain medium index', f.n_medium],
        ['Upper-level lifetime', si(f.lifetime, 's')], ['Quantum yield', f.quantum_yield],
        ['Spontaneous emission factor β', f.beta],
        ['Molecule density', f.molecule_density === 0 ? 'unlimited' : `${f.molecule_density} m⁻³`],
        ['Emission wavelength', si(f.emission_wavelength, 'm')],
        ['Cells in gain medium', f.cells_in_gain],
    ].map(([k, v]) => `<div><dt>${k}</dt><dd>${v}</dd></div>`).join('');
}

// --- Running ---------------------------------------------------------------
// One run at a time; a change during a run queues only the latest parameters.

const worker = new Worker(new URL('./worker.js', import.meta.url), { type: 'module' });
let ready = false;
let busy = false;
let pending = null;
let nextId = 0;

function setStatus(text, kind = '') {
    status.textContent = text;
    status.className = `status ${kind}`;
}

function request() {
    let params;
    try {
        params = readParameters();
    } catch (error) {
        setStatus(error.message, 'error');
        return;
    }
    pending = params;
    if (ready && !busy) dispatch();
}

function dispatch() {
    busy = true;
    output.classList.add('running');
    worker.postMessage({ id: ++nextId, params: pending });
    pending = null;
}

worker.onmessage = (event) => {
    const message = event.data;
    if (message.ready) {
        ready = true;
        if (pending) dispatch();
        return;
    }
    if (message.failed) {
        setStatus(`Could not load the simulation: ${message.failed}`, 'error');
        return;
    }
    busy = false;
    output.classList.remove('running');
    const result = message.result;
    if (result.ok) {
        render(result);
        setStatus(`${result.time.length.toLocaleString()} steps in ${result.elapsed_ms.toFixed(0)} ms`);
        output.classList.remove('stale');
    } else {
        setStatus(result.error, 'error');
        output.classList.add('stale');
    }
    if (pending) dispatch();
};
worker.onerror = (event) => setStatus(`Could not load the simulation: ${event.message}`, 'error');

let timer = null;
function schedule() {
    clearTimeout(timer);
    timer = setTimeout(request, 120);
}

const lengthNumber = form.elements.cavity_length;
const lengthRange = form.elements.cavity_length_range;
lengthRange.addEventListener('input', () => { lengthNumber.value = lengthRange.value; });
lengthNumber.addEventListener('input', () => { lengthRange.value = lengthNumber.value; });

form.addEventListener('input', () => {
    showModeFields();
    schedule();
});
form.addEventListener('submit', (event) => event.preventDefault());
document.getElementById('reset').addEventListener('click', () => {
    for (const [key, value] of defaults) {
        form.elements[key].value = value;
    }
    lengthRange.value = lengthNumber.value;
    showModeFields();
    request();
});

showModeFields();
request();

// A small dependency-free time-series plot on a <canvas>: up to two y axes,
// optional log scale, min/max decimation per pixel column and a hover readout.

const DPR = () => window.devicePixelRatio || 1;

function cssVar(element, name) {
    return getComputedStyle(element).getPropertyValue(name).trim();
}

/// Formats a number with an SI prefix, e.g. 1.2e-9 -> "1.2 n".
export function si(value, unit = '', digits = 3) {
    if (!Number.isFinite(value)) return '–';
    if (unit === '') return scientific(value, digits);
    if (value === 0) return `0 ${unit}`.trim();
    const prefixes = { '-15': 'f', '-12': 'p', '-9': 'n', '-6': 'µ', '-3': 'm', '0': '',
                       '3': 'k', '6': 'M', '9': 'G', '12': 'T' };
    let exponent = Math.floor(Math.log10(Math.abs(value)) / 3) * 3;
    exponent = Math.max(-15, Math.min(12, exponent));
    const scaled = value / 10 ** exponent;
    return `${Number(scaled.toPrecision(digits))} ${prefixes[exponent]}${unit}`.trim();
}

const SUPERSCRIPT = { '-': '⁻', 0: '⁰', 1: '¹', 2: '²', 3: '³', 4: '⁴', 5: '⁵', 6: '⁶', 7: '⁷', 8: '⁸', 9: '⁹' };

/// Formats a plain count, e.g. 1.2e12 -> "1.2×10¹²". Used when there is no unit,
/// because SI prefixes on counts ("1 T molecules") read badly.
export function scientific(value, digits = 3) {
    if (value === 0) return '0';
    const exponent = Math.floor(Math.log10(Math.abs(value)));
    if (exponent >= -2 && exponent < 4) return String(Number(value.toPrecision(digits)));
    const mantissa = Number((value / 10 ** exponent).toPrecision(digits));
    const power = `10${[...String(exponent)].map((c) => SUPERSCRIPT[c]).join('')}`;
    return mantissa === 1 ? power : `${mantissa}×${power}`;
}

function niceStep(range, count) {
    const raw = range / count;
    const magnitude = 10 ** Math.floor(Math.log10(raw));
    const residual = raw / magnitude;
    const nice = residual > 5 ? 10 : residual > 2 ? 5 : residual > 1 ? 2 : 1;
    return nice * magnitude;
}

function linearTicks(min, max, count) {
    if (!(max > min)) return [min];
    const step = niceStep(max - min, count);
    const ticks = [];
    for (let v = Math.ceil(min / step) * step; v <= max + step * 1e-9; v += step) {
        ticks.push(Math.abs(v) < step * 1e-9 ? 0 : v);
    }
    return ticks;
}

function logTicks(min, max) {
    const ticks = [];
    const first = Math.ceil(Math.log10(min));
    const last = Math.floor(Math.log10(max));
    const stride = Math.max(1, Math.ceil((last - first + 1) / 8));
    for (let e = first; e <= last; e += stride) ticks.push(10 ** e);
    return ticks;
}

function extent(values, log) {
    let min = Infinity;
    let max = -Infinity;
    for (const v of values) {
        if (!Number.isFinite(v) || (log && v <= 0)) continue;
        if (v < min) min = v;
        if (v > max) max = v;
    }
    if (min === Infinity) return log ? [1e-3, 1] : [0, 1];
    if (log) {
        // Keep the useful dynamic range; the spontaneous-emission floor
        // otherwise squeezes the pulses into the top of the plot.
        min = Math.max(min, max * 1e-9);
        return [min, max * 1.5];
    }
    if (min > 0) min = 0;
    if (max === min) max = min + 1;
    return [min, max + (max - min) * 0.05];
}

export class TimePlot {
    /// options: { xLabel, axes: { left: {label, unit}, right?: {label, unit} } }
    constructor(container, options) {
        this.options = options;
        this.container = container;
        this.canvas = document.createElement('canvas');
        this.readout = document.createElement('div');
        this.readout.className = 'plot-readout';
        container.append(this.canvas, this.readout);
        this.data = null;
        this.hoverX = null;
        this.logLeft = false;

        new ResizeObserver(() => this.draw()).observe(container);
        this.canvas.addEventListener('pointermove', (event) => {
            const rect = this.canvas.getBoundingClientRect();
            this.hoverX = event.clientX - rect.left;
            this.draw();
        });
        this.canvas.addEventListener('pointerleave', () => {
            this.hoverX = null;
            this.readout.textContent = '';
            this.draw();
        });
        matchMedia('(prefers-color-scheme: dark)').addEventListener('change', () => this.draw());
    }

    /// data: { x: Float64Array, series: [{ y, label, unit, axis: 'left'|'right', color }] }
    setData(data) {
        this.data = data;
        this.draw();
    }

    /// Series colours may be CSS custom properties, resolved on every draw so
    /// they follow a light/dark switch.
    color(s) {
        return s.color.startsWith('--') ? cssVar(this.container, s.color) : s.color;
    }

    setLogLeft(log) {
        this.logLeft = log;
        this.draw();
    }

    draw() {
        const width = this.container.clientWidth;
        const height = this.container.clientHeight;
        if (width === 0 || height === 0) return;
        const ratio = DPR();
        this.canvas.width = Math.round(width * ratio);
        this.canvas.height = Math.round(height * ratio);
        this.canvas.style.width = `${width}px`;
        this.canvas.style.height = `${height}px`;
        const ctx = this.canvas.getContext('2d');
        ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
        ctx.clearRect(0, 0, width, height);

        const text = cssVar(this.container, '--text-muted');
        const grid = cssVar(this.container, '--grid');
        const font = '12px system-ui, -apple-system, sans-serif';
        ctx.font = font;

        const hasRight = Boolean(this.options.axes.right);
        const pad = { left: 64, right: hasRight ? 64 : 16, top: 12, bottom: 40 };
        const plotW = Math.max(10, width - pad.left - pad.right);
        const plotH = Math.max(10, height - pad.top - pad.bottom);

        if (!this.data || this.data.x.length < 2) return;
        const { x, series } = this.data;
        const xMin = x[0];
        const xMax = x[x.length - 1];
        const sx = (v) => pad.left + ((v - xMin) / (xMax - xMin)) * plotW;

        const scales = {};
        for (const side of hasRight ? ['left', 'right'] : ['left']) {
            const log = side === 'left' && this.logLeft;
            const values = series.filter((s) => s.axis === side).flatMap((s) => [...extent(s.y, log)]);
            const [lo, hi] = [Math.min(...values), Math.max(...values)];
            const toY = log
                ? (v) => pad.top + plotH - ((Math.log10(Math.max(v, lo)) - Math.log10(lo)) /
                                            (Math.log10(hi) - Math.log10(lo))) * plotH
                : (v) => pad.top + plotH - ((v - lo) / (hi - lo)) * plotH;
            scales[side] = { lo, hi, log, toY, ticks: log ? logTicks(lo, hi) : linearTicks(lo, hi, 5) };
        }

        // Grid and axes.
        ctx.strokeStyle = grid;
        ctx.lineWidth = 1;
        ctx.fillStyle = text;
        ctx.textAlign = 'center';
        ctx.textBaseline = 'top';
        for (const tick of linearTicks(xMin, xMax, Math.max(2, Math.floor(plotW / 90)))) {
            const px = Math.round(sx(tick)) + 0.5;
            ctx.beginPath();
            ctx.moveTo(px, pad.top);
            ctx.lineTo(px, pad.top + plotH);
            ctx.stroke();
            ctx.fillText(si(tick, 's', 3), px, pad.top + plotH + 6);
        }
        ctx.fillText(this.options.xLabel, pad.left + plotW / 2, pad.top + plotH + 22);

        ctx.textBaseline = 'middle';
        const left = scales.left;
        ctx.textAlign = 'right';
        for (const tick of left.ticks) {
            const py = Math.round(left.toY(tick)) + 0.5;
            ctx.beginPath();
            ctx.moveTo(pad.left, py);
            ctx.lineTo(pad.left + plotW, py);
            ctx.stroke();
            ctx.fillText(si(tick, this.options.axes.left.unit, 2), pad.left - 6, py);
        }
        if (hasRight) {
            ctx.textAlign = 'left';
            for (const tick of scales.right.ticks) {
                ctx.fillText(si(tick, this.options.axes.right.unit, 2), pad.left + plotW + 6,
                             scales.right.toY(tick));
            }
        }

        // Series, decimated to the min and max per pixel column.
        ctx.save();
        ctx.beginPath();
        ctx.rect(pad.left, pad.top, plotW, plotH);
        ctx.clip();
        for (const s of series) {
            const toY = scales[s.axis].toY;
            ctx.strokeStyle = this.color(s);
            ctx.lineWidth = 1.5;
            ctx.lineJoin = 'round';
            ctx.beginPath();
            let column = null;
            let lo = 0;
            let hi = 0;
            let first = true;
            const flush = () => {
                if (column === null) return;
                if (first) {
                    ctx.moveTo(column, lo);
                    first = false;
                } else {
                    ctx.lineTo(column, lo);
                }
                if (hi !== lo) ctx.lineTo(column, hi);
            };
            for (let i = 0; i < x.length; i++) {
                const px = Math.round(sx(x[i]) * ratio) / ratio;
                const py = toY(s.y[i]);
                if (px !== column) {
                    flush();
                    column = px;
                    lo = hi = py;
                } else {
                    lo = Math.min(lo, py);
                    hi = Math.max(hi, py);
                }
            }
            flush();
            ctx.stroke();
        }
        ctx.restore();

        // Hover readout.
        if (this.hoverX !== null && this.hoverX >= pad.left && this.hoverX <= pad.left + plotW) {
            const t = xMin + ((this.hoverX - pad.left) / plotW) * (xMax - xMin);
            let lo = 0;
            let hi = x.length - 1;
            while (hi - lo > 1) {
                const mid = (lo + hi) >> 1;
                if (x[mid] < t) lo = mid; else hi = mid;
            }
            const i = t - x[lo] < x[hi] - t ? lo : hi;
            const px = Math.round(sx(x[i])) + 0.5;
            ctx.strokeStyle = text;
            ctx.setLineDash([3, 3]);
            ctx.beginPath();
            ctx.moveTo(px, pad.top);
            ctx.lineTo(px, pad.top + plotH);
            ctx.stroke();
            ctx.setLineDash([]);
            for (const s of series) {
                ctx.fillStyle = this.color(s);
                ctx.beginPath();
                ctx.arc(px, scales[s.axis].toY(s.y[i]), 3, 0, 2 * Math.PI);
                ctx.fill();
            }
            this.readout.innerHTML = [`t = ${si(x[i], 's', 4)}`,
                ...series.map((s) => `<span style="color:${this.color(s)}">●</span> ${s.label}: ${si(s.y[i], s.unit, 4)}`)]
                .join('<br>');
        }
    }
}

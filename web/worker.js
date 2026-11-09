// Runs the WebAssembly simulation off the main thread so the page stays
// responsive for long cavities.
import createCavdyn from './cavdyn.js';

const ready = createCavdyn();

self.onmessage = async (event) => {
    const { id, params } = event.data;
    try {
        const module = await ready;
        const start = performance.now();
        const result = module.simulate(params);
        result.elapsed_ms = performance.now() - start;
        const transfer = result.ok
            ? ['time', 'output_power', 'pump_power', 'cavity_photons', 'total_inversion']
                .map((key) => result[key].buffer)
            : [];
        self.postMessage({ id, result }, transfer);
    } catch (error) {
        self.postMessage({ id, result: { ok: false, error: String(error) } });
    }
};

ready.then(() => self.postMessage({ ready: true }),
           (error) => self.postMessage({ failed: String(error) }));

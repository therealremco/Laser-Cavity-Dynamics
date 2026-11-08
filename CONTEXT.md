# cavity-dynamics

A simulation of the time-dependent output of a short laser cavity with a spatially
inhomogeneous gain medium. It is aimed at the regime where the cavity round-trip time is
comparable to the excited-state lifetime, so that a short pump pulse produces a *train* of
output pulses rather than a single smooth pulse: the first pulse depletes part of the
inversion, the remaining inversion amplifies the reflected light on the next round trip, and
so on.

The reference case is a solution of a conjugated polymer in a glass cuvette between a
high reflector and an output coupler, side-pumped by 400 nm, 100 fs pulses focused to a line
along the cavity axis.

This is a rewrite of `cavity-dynamics.cpp` (1999, see `Specifications.txt`). The new version
is a clean C++ project with a core library that has no user-interface code, so that a GUI
can be added later.

## Physical model

### Scope and assumptions

- **One dimension.** The cavity is modelled along its optical axis only. Transverse effects
  are folded into one parameter, the effective mode area `A`, which is used only to convert
  between areal quantities (photons per m²) and absolute quantities (photons, joules, watts).
- **Single wavelength.** All lasing photons have the emission wavelength `λ_L`. There is no
  spectrum, no gain narrowing and no longitudinal mode structure.
- **Ideal four-level gain medium** (Siegman's 2* = 1). The lower laser level empties
  instantly, so each stimulated emission reduces the inversion by exactly one. This suits
  conjugated polymers, which lase on a vibronic transition.
- **Two counter-propagating photon populations**, travelling to the right (+) and to the
  left (−). Interference between them (spatial hole burning) is ignored.
- **No ground-state or excited-state absorption** at the lasing wavelength.
- **Pump absorption is not modelled spatially.** The pump deposits inversion uniformly over
  the pumped stripe; the absorbed fraction is a parameter.

### Relation to Siegman

Siegman's coupled atomic-cavity rate equations (*Lasers*, 1986, p. 508) describe the
*total* photon number `n` in the cavity mode and the *total* inversion `N`:

    dn/dt = K·N·(n + 1) − γ_c·n
    dN/dt = R_p − γ_2·N − K·N·n

with `K = σ·c / (n_ref·V)`. Here, instead, the cavity is cut into cells and every cell carries
its own inversion density and its own right- and left-moving photon populations. Each
term of Siegman's equations has a local counterpart:

| Siegman (lumped)             | This model (per cell, per time step)                              |
| ---------------------------- | ----------------------------------------------------------------- |
| `K·N·n` (stimulated)         | Amplification of the photon packet passing through the cell      |
| `K·N` (the "+1")             | Spontaneous emission seed, scaled by the coupling factor `β`      |
| `γ_c·n` (cavity loss)        | Internal loss `α` per cell **plus** explicit mirror reflectivities |
| `γ_2·N`                      | Exponential decay of the inversion with lifetime `τ_2`            |
| `R_p`                        | Pump deposition, with a time profile and spatial extent           |

Siegman's `γ_c` contains both mirror and internal losses. Because the mirrors are explicit
here, `α` must contain **only** the distributed internal loss (scattering, cuvette
absorption). The legacy code applied `γ_c` per cell *and* the mirror reflectivities, which
counted the mirror loss twice.

In the limit of a cavity completely filled with homogeneous gain, the model must reduce to
Siegman's equations (see Validation).

## Geometry

Positions `z` run from the high reflector (`z = 0`) to the output coupler (`z = L_cav`).

```
 HR                                                              OC
 |        background         |  gain medium (cuvette)  |  background  |
 |                           |      |  pumped   |       |              |
 z=0                       z_g     z_p    z_p+L_p   z_g+L_g          L_cav
```

- The gain medium occupies `[z_g, z_g + L_g]` and has refractive index `n_g`.
- The rest of the cavity has background index `n_b` (1.0 for air).
- The pump stripe covers `[z_p, z_p + L_p]` and must lie inside the gain medium.
- Cuvette walls are not modelled separately; their optical path can be absorbed into `n_b`
  or `L_cav`.

## Parameters

All values are in SI units internally. Defaults reproduce the legacy run where possible.

### Cavity

| Name       | Symbol   | Unit | Default | Meaning                                        |
| ---------- | -------- | ---- | ------- | ---------------------------------------------- |
| length     | `L_cav`  | m    | 1.5e-2  | Mirror-to-mirror distance                      |
| r_hr       | `R_HR`   | –    | 1.0     | Reflectivity of the high reflector (z = 0)     |
| r_oc       | `R_OC`   | –    | 0.95    | Reflectivity of the output coupler (z = L_cav) |
| n_background | `n_b`  | –    | 1.0     | Refractive index outside the gain medium       |
| mode_area  | `A`      | m²   | 1e-8    | Effective cross-section of the lasing mode     |

### Gain medium

| Name           | Symbol   | Unit | Default | Meaning                                               |
| -------------- | -------- | ---- | ------- | ----------------------------------------------------- |
| start          | `z_g`    | m    | 3.0e-3  | Position of the gain medium's left edge               |
| length         | `L_g`    | m    | 9.0e-3  | Length of the gain medium                             |
| n_medium       | `n_g`    | –    | 1.5     | Refractive index of the gain medium                   |
| cross_section  | `σ`      | m²   | 1e-20   | Stimulated-emission cross section at `λ_L`            |
| lifetime       | `τ_2`    | s    | 1e-9    | Excited-state (photoluminescence) lifetime, `γ_2 = 1/τ_2` |
| quantum_yield  | `Φ`      | –    | 1.0     | Fraction of decays that are radiative                 |
| beta           | `β`      | –    | 1e-4    | Fraction of spontaneous emission coupled into the lasing mode (both directions together) |
| internal_loss  | `α`      | m⁻¹  | 0.0     | Distributed loss inside the gain medium               |
| molecule_density | `N_mol`| m⁻³  | 0 (off) | Upper bound on the inversion (ground-state depletion) |
| emission_wavelength | `λ_L` | m  | 5.5e-7  | Lasing wavelength, sets the photon energy `h·c/λ_L`   |

The legacy parameter `K = 2e-12` corresponds to `σ·c/n_g`, so `σ = K·n_g/c = 1e-20 m²`.
The legacy `γ_c = 1e9 s⁻¹` corresponds to `α = γ_c·n_g/c = 5 m⁻¹`, but because it also
contained the mirror loss the default here is 0.

### Pump

| Name            | Symbol  | Unit | Default  | Meaning                                              |
| --------------- | ------- | ---- | -------- | ---------------------------------------------------- |
| wavelength      | `λ_p`   | m    | 4.0e-7   | Pump wavelength, sets the pump photon energy         |
| start           | `z_p`   | m    | 3.0e-3   | Position of the pump stripe's left edge              |
| length          | `L_p`   | m    | 9.0e-3   | Length of the pump stripe                            |
| pulse_energy    | `E_p`   | J    | –        | Pump energy per pulse                                |
| pulse_duration  | `t_p`   | s    | 1e-13    | FWHM of a Gaussian pulse; `0` = instantaneous; `inf` = continuous |
| power           | `P_p`   | W    | –        | Pump power, only for continuous pumping              |
| delay           | `t_0`   | s    | 0        | Arrival time of the pulse peak                       |
| absorbed_fraction | `f_abs` | –  | 1.0      | Fraction of pump energy absorbed in the pumped volume |
| efficiency      | `η`     | –    | 0.8      | Fraction of absorbed pump photons that end up in the upper laser level |
| initial_inversion | `N_0` | m⁻³  | 1e23     | Alternative to `E_p`: set the inversion directly at `t_0` |

At most one of `pulse_energy`, `power` (with `t_p = inf`) or `initial_inversion` may be given;
if none is, `initial_inversion` takes its default.
`initial_inversion` reproduces the legacy behaviour (an instantaneous step-function
inversion of `1e23 m⁻³`).

### Simulation

| Name             | Unit | Default | Meaning                                                |
| ---------------- | ---- | ------- | ------------------------------------------------------ |
| cells_in_gain    | –    | 60      | Number of cells in the gain medium; sets `Δt`          |
| duration         | s    | –       | Simulated time; alternatively `round_trips`            |
| round_trips      | –    | 25      | Simulated time in cavity round trips                   |
| snapshot_interval | steps | 0 (off) | Write spatial profiles every this many steps          |

The time step is **not** a free parameter: it is fixed by the grid (see below).

## Numerical method

### Grid

The grid uses cells of **equal transit time** `Δt`, so that light moves exactly one cell per
step in every region and no interpolation is needed:

    Δt   = n_g · (L_g / cells_in_gain) / c
    Δx_i = c · Δt / n_i                      (n_i = n_g or n_b)

The background regions get `round(L / Δx_b)` cells. Rounding changes their lengths slightly;
the simulator reports the actual cavity length and round-trip time. The round-trip time is
`2 · M · Δt` for `M` cells in total (see Mirrors).

### State

Per cell `i`:

- `N_i` — inversion density (m⁻³)
- `J⁺_i`, `J⁻_i` — right- and left-moving photon populations, stored as **photons per m²**.
  This areal density is conserved when a packet moves into a cell of different length.

### One time step

1. **Pump.** Add the inversion deposited during `[t, t + Δt]` to every cell inside the pump
   stripe. The deposited density for a pulse is

       ΔN = η · f_abs · (E_p / (h·c/λ_p)) / (L_p · A) · w(t)

   where `w(t)` is the fraction of the Gaussian pulse energy inside the step (exactly 1 in
   the step containing `t_0` for an instantaneous pulse). The simulation starts at `t = 0`;
   the first step also receives the part of the pulse before `t = 0`, so a pulse centred on
   `t_0 = 0` deposits its full energy. For continuous pumping replace
   `E_p · w(t)` with `P_p · Δt`. With `initial_inversion`, `ΔN = N_0` in the step containing
   `t_0`. Cells partly covered by the stripe receive `ΔN` times their covered fraction.
   If `N_mol > 0`, clamp `N_i ≤ N_mol`.

2. **Gain and depletion (Frantz–Nodvik).** Each packet passing a gain cell is amplified with
   the exact solution for a pulse traversing a slab of a four-level medium:

       G_0   = exp(σ · N_i · Δx_i)
       J_out = (1/σ) · ln(1 + G_0 · (exp(σ · J_in) − 1))
       N_i  ← N_i − (J_out − J_in) / Δx_i

   For small `σ·J` this reduces to `J_out = G_0 · J_in`, Siegman's linear term. It conserves
   photons plus inversion exactly and can never drive `N_i` negative, which replaces the
   legacy forward-Euler update (about 15 % gain per step, not converged). Apply it to the
   two directions one after the other, alternating which goes first every step to avoid a
   directional bias.

3. **Spontaneous seed.** Add to each direction

       ΔJ = (β/2) · Φ · N_i · (Δt/τ_2) · Δx_i

4. **Loss.** Multiply both packets by `exp(−α_i · Δx_i)`.

5. **Inversion decay.** `N_i ← N_i · exp(−Δt/τ_2)`. This includes the spontaneous emission
   that seeded the mode.

6. **Propagation.** Shift `J⁺` one cell to the right and `J⁻` one cell to the left.

7. **Mirrors.** The right-moving packet that was in the last cell becomes the left-moving
   packet in the last cell, multiplied by `R_OC`; the transmitted part is the output (below).
   Likewise at the high reflector with `R_HR`. A reflection takes one step, which makes the
   round trip exactly `2·M·Δt` and equal to `2·Σ n_i·Δx_i / c`.

All updates within a step use the state at the start of the step for neighbouring cells.

### Output quantities

Per step:

- Output photons: `Q_out = (1 − R_OC) · J⁺_{M−1} · A`
- Output power: `P_out = Q_out · (h·c/λ_L) / Δt`
- Intracavity photons: `Σ (J⁺_i + J⁻_i) · A`
- Total inversion: `Σ N_i · Δx_i · A`

Derived after the run: the individual pulses in the train (peak time, peak power, energy,
FWHM) and the total output energy.

## Software architecture

### Principles

- C++17, built with CMake on macOS (clang) and Linux (gcc). No dependencies for the core
  library.
- `core` contains all physics and no I/O, no `std::cout`, no global state. A GUI or a test
  links the same library as the command-line program.
- Each class has a header in `include/cavdyn/` and an implementation in `src/`.
- Namespace `cavdyn`. SI units throughout; names carry no unit suffixes but every parameter is
  documented with its unit in the header.
- Parameters are validated once, when a `Simulator` is constructed. Invalid input throws
  `cavdyn::ParameterError` with a message naming the parameter.

### Layout

```
cavity-dynamics/
  CMakeLists.txt
  CONTEXT.md
  include/cavdyn/
    Constants.h         c, h
    Parameters.h        CavityParams, GainMediumParams, PumpParams, SimulationParams
    Grid.h              Cell layout, Δt, region per cell
    Cavity.h            Mirrors, internal loss, output coupling
    GainMedium.h        Inversion field, Frantz–Nodvik step, decay, seed
    Pump.h              Deposition per step for pulsed, continuous or direct inversion
    Simulator.h         Owns the state, runs steps, reports results
    Observer.h          Callback interface for progress and snapshots
    Results.h           Time series, snapshots, pulse analysis
  src/
    (one .cpp per header above; Constants.h and Observer.h are header-only)
  io/
    ConfigReader.h/.cpp   Reads parameters from a key = value file
    ResultWriter.h/.cpp   Writes .dat files with a parameter header
  apps/cli/
    main.cpp            Command-line front end
  tests/
    TestSupport.h         Shared checks and a runner (`<exe> [test name]`)
    test_io.cpp           Configuration and result files
    test_components.cpp   Parameters, Grid, GainMedium, Cavity, Pump in isolation
    test_simulator.cpp    Stepping, pumping, observers, snapshots, pulse analysis
    test_validation.cpp   The validation cases below, one CTest test each
```

The 1999 program and its specification are kept locally in `legacy/`, which is not
tracked by git; the original source is in the history (commit `8d7fbda`,
`cavity-dynamics.cpp`). The legacy comparison test reproduces its geometry from the
parameters and does not need the old program.

`io` is a separate library so that a GUI can choose its own formats.

### Main classes

**`Grid`** — built from the parameters; knows `Δt`, the number of cells, each cell's length,
index and region (background, gain, pumped gain), and the actual cavity length and round-trip
time after rounding.

**`Cavity`** — the mirrors and the internal loss per cell. Applies reflection at both ends and
returns the output photons for the step.

**`GainMedium`** — owns `N_i`. Provides the Frantz–Nodvik amplification of a packet in a
cell, the spontaneous seed and the decay.

**`Pump`** — returns the inversion density to add to each cell for the interval
`[t, t + Δt]`.

**`Simulator`** — owns the grid, the components and the state (`N`, `J⁺`, `J⁻`):

```cpp
class Simulator {
public:
    explicit Simulator(const Parameters& params);   // validates, builds grid
    void step();                                     // one Δt
    void run(Observer* observer = nullptr);          // until the requested duration
    void set_photons(std::size_t cell, double right_moving, double left_moving);
    double time() const;
    std::size_t step_index() const;
    const Grid& grid() const;
    const Parameters& parameters() const;            // with the default pump mode filled in
    StateView state() const;                         // read-only N, J⁺, J⁻ for plotting
    const Results& results() const;
};
```

Method names are `snake_case` throughout. `set_photons` sets a starting packet instead of
waiting for the spontaneous seed; the propagation and conservation tests use it.

**`Observer`** — the hook for a GUI or a progress bar. `run()` calls it after every step
with that step's results and a `StateView` of the current state, which is valid only during
the call. Snapshots requested with `snapshot_interval` are collected in `Results`
independently. Returning `false` cancels the run.

**`Results`** — the per-step time series (time, output photons, output power, intracavity
photons, total inversion), the snapshots, and a function that splits the output into pulses.
Two peaks are separate pulses when the power between them drops below half the smaller
peak; peaks below 0.1 % of the highest are ignored (both thresholds are arguments). Each
pulse runs from the lowest point before its peak to the lowest point after it, so the pulse
energies add up to the total output energy.

### Command-line program

```
cavity-dynamics run config.txt [-o output.dat]
cavity-dynamics defaults > config.txt
```

The configuration is a plain `key = value` file with `#` comments and section headers
(`[cavity]`, `[gain_medium]`, `[pump]`, `[simulation]`); keys match the *Name* columns above.
Unknown keys are an error. Every key is optional: the file overrides the defaults in
`Parameters.h`, which are the only place default values are defined. `defaults` writes
them from a default-constructed `Parameters`, each key with its unit, and unset optional
keys (the pump modes, `duration`) commented out. The program prints the derived grid values (`Δt`, cell counts per
region, actual cavity length, round-trip time) before running. The file written by
`defaults` lists the three pump modes commented out, so enabling one does not clash with
the default `initial_inversion`.

### Output file

Tab-separated text, readable by gnuplot, Origin and Python:

- A header of `#` lines with the program version (from CMake's `PROJECT_VERSION`), the
  date and time, every parameter as used (including the default pump mode) and the derived
  grid values.
- One row per time step: `time_s  output_photons  output_power_W  cavity_photons
  total_inversion`.
- With snapshots enabled, a second file `<name>.snapshots.dat` with blocks of
  `z_m  N  J_plus  J_minus`, separated by blank lines and labelled with their time.
- A short summary at the end of the run on screen: pulses found, their energies and spacing.

## Validation

Each of these is a test in `tests/`:

1. **Lumped limit.** Gain fills the whole cavity, uniform pump, small cells. The total photon
   number and inversion must match an ODE integration of Siegman's equations with
   `K = σ·c/(n_g·V)` and `γ_c = (c/(2·n_g·L)) · ln(1/(R_HR·R_OC)) + α·c/n_g`, including the
   threshold `N_th = γ_c/K`.
2. **Relaxation oscillations.** Continuous pumping above threshold: the frequency of the
   damped oscillations must approach `√((r−1)·γ_c·γ_2)`.
3. **Conservation.** With `R_HR = R_OC = 1`, `α = 0` and `τ_2 = ∞`: photons plus inversion
   are constant to rounding error.
4. **Propagation.** A single packet without gain returns to its starting cell after exactly
   `2·M` steps, reduced by `R_HR · R_OC`.
5. **Convergence.** Doubling `cells_in_gain` changes the pulse peak times by less than one
   step and the pulse energies by less than 1 %.
6. **Legacy comparison.** With `initial_inversion = 1e23`, the legacy geometry, `n_b = 1.5`
   (the legacy code used one index throughout) and the legacy `K`, the pulse spacing matches the old program (one pulse every half round trip at the
   output, because the left- and right-moving trains are symmetric).

## Open questions

- **GUI toolkit.** Qt is the obvious choice for a native macOS application with plots; a
  SwiftUI front end calling the library is an alternative. The core API above does not depend
  on the choice.
- **Pump absorption profile.** The pump enters from the side and is absorbed with depth into
  the cuvette. If the mode only samples part of that depth, `f_abs` and `A` must be chosen
  consistently. Modelling the depth profile would need a second dimension.
- **Default `β` and `A`.** These set the build-up time of the first pulse and the absolute
  output power. They should be estimated from the thesis set-up (mode waist, solid angle of
  the mode).
- **Ground-state absorption at `λ_L`.** Ignored for now; conjugated polymers have small
  overlap between absorption and emission, but it can be added as a loss proportional to
  `N_mol − N_i`.

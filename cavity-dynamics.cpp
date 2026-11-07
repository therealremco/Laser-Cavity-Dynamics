// cavity-dynamics.cpp
// Origingal version 2.2.061099
// Updated to compile on MacOS on 2026-11-07
// R. van Schoonderwoerd den Bezemer
//
// See Specifications.txt for details.

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

// Declarations:
const int num_timesteps(100);    // number of time-steps in calculation
const int num_xsteps(100);       // number of units length
const int num_roundtrips(25);    // number of roundtrips
double inv_max(1e23);            // inversion at t=0

int inv_l_border;                // left side of inversion step function
int inv_r_border;                // right side
int num_calcs;                   // num_timesteps times num_roundtrips

double delta_t;                  // time for light to travel one step
double delta_x;                  // unit length

double cavity(1.5e-2);            // cavity length
double ref_index(1.5);            // average refractive index
double c(3e8);                    // speed of light
double gamma_c(1e9);              // cavity decay rate
double gamma_2(1e9);              // inversion decay rate
double K(2e-12);                  // spontaneous emission coefficient
double R_HR(1.0);                 // reflectivity of high reflector
double R_OC(0.95);                // reflectivity of output coupler

// Arrays for keeping track of photon numbers:
double n_left[num_xsteps];        // photons travelling left
double n_right[num_xsteps];       // photons travelling right
double inversion[num_xsteps];     // photons in upper level
// Arrays for situation one time-step earlier:
double old_n_left[num_xsteps];
double old_n_right[num_xsteps];
double old_inversion[num_xsteps];

bool read_parameter(const char* name, double& value);
bool parameters();
void init_var();
void init_arrays(double old_n_l[num_xsteps],
                 double old_n_r[num_xsteps],
                 double old_inv[num_xsteps]);
double calculate(double old_n_l[num_xsteps],
                 double old_n_r[num_xsteps],
                 double old_inv[num_xsteps]);

int main()
{
    if (!parameters())
    {
        return 1;
    }
    init_var();

    std::cout << "!!An existing output file with the same name will be "
              << "overwritten!!" << std::endl << std::endl;
    std::cout << "Please give the date in the following form: DDMMYY"
              << std::endl;

    std::string date;
    if (!(std::cin >> date) || date.size() != 6)
    {
        std::cerr << "Invalid date: enter exactly six digits (DDMMYY)."
                  << std::endl;
        return 1;
    }
    for (char digit : date)
    {
        if (digit < '0' || digit > '9')
        {
            std::cerr << "Invalid date: enter exactly six digits (DDMMYY)."
                      << std::endl;
            return 1;
        }
    }

    std::cout << "Please give a two character identifier for the output file"
              << std::endl;
    std::string file_id;
    if (!(std::cin >> file_id) || file_id.size() != 2)
    {
        std::cerr << "Invalid identifier: enter exactly two letters or digits."
                  << std::endl;
        return 1;
    }
    for (char character : file_id)
    {
        const bool is_letter = (character >= 'A' && character <= 'Z') ||
                               (character >= 'a' && character <= 'z');
        const bool is_digit = character >= '0' && character <= '9';
        if (!is_letter && !is_digit)
        {
            std::cerr
                << "Invalid identifier: enter exactly two letters or digits."
                << std::endl;
            return 1;
        }
    }

    const std::string file_name =
        "Dynamics." + date + "." + file_id + ".dat";
    init_arrays(old_n_left, old_n_right, old_inversion);

    std::ofstream output(file_name);
    if (!output)
    {
        std::cerr << "Could not open output file: " << file_name << std::endl;
        return 1;
    }

    output << std::scientific << std::setprecision(6);
    output << "# Filename:\t " << file_name << "\n";
    output << "# Parameters:\n#  delta_x:\t " << delta_x
           << "\n#  delta_t:\t " << delta_t << "\n";
    output << "#  cavity:\t " << cavity << "\n#  ref_index:\t "
           << std::fixed << std::setprecision(6) << ref_index << "\n";
    output << std::scientific << std::setprecision(6);
    output << "#  gamma_c:\t " << gamma_c << "\n#  gamma_2:\t " << gamma_2
           << "\n#  K:\t\t " << K << "\n";
    output << "#  inv_max:\t " << inv_max << "\n#  R_HR:\t\t "
           << std::fixed << std::setprecision(6) << R_HR
           << "\n#  R_OC:\t\t " << R_OC << "\n";
    output << std::scientific << std::setprecision(6);
    output << "#  num_timesteps:\t " << num_timesteps
           << "\n#  num_xsteps:\t " << num_xsteps
           << "\n#  num_calcs:\t " << num_calcs << "\n";

    for (int t = 0; t < num_calcs; t++)
    {
        const double result =
            calculate(old_n_left, old_n_right, old_inversion);
        output << t << ' ' << result << '\n';
    }

    output.close();
    if (!output)
    {
        std::cerr << "Failed while writing output file: " << file_name
                  << std::endl;
        return 1;
    }
    return 0;
}

bool read_parameter(const char* name, double& value)
{
    std::cout << std::endl << name << " (" << value << ") ";
    if (!(std::cin >> value) || !std::isfinite(value))
    {
        std::cerr << std::endl << "Invalid value for " << name << "."
                  << std::endl;
        return false;
    }
    return true;
}

bool parameters()
{
    std::cout << "Please give the following parameters:" << std::endl;
    const bool valid =
        read_parameter("inv_max", inv_max) &&
        read_parameter("cavity", cavity) &&
        read_parameter("ref_index", ref_index) &&
        read_parameter("gamma_c", gamma_c) &&
        read_parameter("gamma_2", gamma_2) &&
        read_parameter("K", K) &&
        read_parameter("R_HR", R_HR) &&
        read_parameter("R_OC", R_OC);
    std::cout << std::endl;
    return valid;
}

void init_arrays(double old_n_l[num_xsteps],
                 double old_n_r[num_xsteps],
                 double old_inv[num_xsteps])
// This function fills the cavity photon number arrays with zeroes, the
// situation at t=0. It also fills the inversion array, the first fifth
// and the last fifth zero, non-zero in between.
{
    for (int a = 0; a < num_xsteps; a++)
    {
        old_n_l[a] = 0;
        old_n_r[a] = 0;
    }
    for (int b = 0; b < inv_l_border; b++)
    {
        old_inv[b] = 0;
    }
    for (int index = inv_l_border; index < inv_r_border; index++)
    {
        old_inv[index] = inv_max;
    }
    for (int d = inv_r_border; d < num_xsteps; d++)
    {
        old_inv[d] = 0;
    }
}

void init_var()
// This function initializes some variables.
{
    delta_x = cavity / num_xsteps;
    delta_t = ref_index * delta_x / c;
    inv_l_border = num_xsteps / 5;
    inv_r_border = 4 * num_xsteps / 5;
    num_calcs = num_timesteps * num_roundtrips;
}

double calculate(double old_n_l[num_xsteps],
                 double old_n_r[num_xsteps],
                 double old_inv[num_xsteps])
// This function houses the main algoritm for calculation.
{
    for (int x = 0; x < num_xsteps; x++)
    {
        // Calculate new inversion:
        inversion[x] = old_inv[x] - (delta_t * gamma_2 * old_inv[x])
                       - (delta_t * K * old_inv[x] *
                          (old_n_l[x] + old_n_r[x]));
        // Calculate new number of photons travelling to the left:
        if (x < num_xsteps - 1)
        {
            n_left[x] = old_n_l[x + 1] +
                        (delta_t * K * old_inv[x + 1] * old_n_l[x + 1]) +
                        (delta_t * K * old_inv[x]) -
                        (delta_t * gamma_c * old_n_l[x]);
        }
        // Calculate new number of photons travelling to the right:
        if (x > 0)
        {
            n_right[x] = old_n_r[x - 1] +
                         (delta_t * K * old_inv[x - 1] * old_n_r[x - 1]) +
                         (delta_t * K * old_inv[x]) -
                         (delta_t * gamma_c * old_n_r[x]);
        }
    }
    // The next step reflects the photons in the outermost array elements
    // of the cavity mirrors:
    n_right[0] = R_HR * old_n_l[0];
    n_left[num_xsteps - 1] = R_OC * old_n_r[num_xsteps - 1];
    // Put the new numbers in the arrays for the old numbers for the
    // next step:
    for (int i = 0; i < num_xsteps; i++)
    {
        old_inv[i] = inversion[i];
        old_n_l[i] = n_left[i];
        old_n_r[i] = n_right[i];
    }
    // Function returns the number of photons just inside the output
    // coupler:
    return n_left[0];
}

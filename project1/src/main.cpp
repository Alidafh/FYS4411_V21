#include <iostream>         // Printing.
#include <cmath>            // Math library.
#include <random>           // RNG.
#include <fstream>          // Write to file.
#include <iomanip>          // Data formatting when writing to file.
#include <chrono>           // Timing.
#include <armadillo>        // Linear algebra.
#include "omp.h"            // Parallelization.
#include "wave_function.cpp"
#include "other_functions.cpp"

#include <sstream>
#include <string>

// These are currently defined in 'other_functions.cpp'.
// const double hbar = 1;
// const double m = 1;
// const double omega = 1;
const double beta = 1;

class VMC
{
private:
    std::string fpath;              // Path to output text file.
    std::ofstream outfile;          // Output file.
    const int n_variations = 100;   // Number of variations.
    const int n_mc_cycles = 70;    // Number of MC cycles.
    const int seed = 1337;          // RNG seed.
    const int n_particles = 100;    // Number of particles.
    const int n_dims = 3;           // Number of spatial dimensions.
    const double step_size = 1;
    const double alpha_step = 0.02;

    const double diffusion_coeff = 0.5;
    // const double time_step = 0.005; // time steps in range [0.0001, 0.001] stable?
    // const double time_step = 0.4;
    double time_step;

    double e_expectation_squared;   // Square of the energy expectation value.
    double energy_step;             // Energy step size.
    double exponential_diff;        // Difference of the exponentials, for Metropolis.

    arma::Mat<double> pos_new = arma::Mat<double>(n_dims, n_particles);         // Proposed new position.
    arma::Mat<double> pos_current = arma::Mat<double>(n_dims, n_particles);     // Current position.
    arma::Col<double> wave_current = arma::Col<double>(n_particles);            // Current wave function.
    arma::Col<double> wave_new = arma::Col<double>(n_particles);                // Proposed new wave function.
    arma::Col<double> e_variances = arma::Col<double>(n_variations);            // Energy variances.
    arma::Col<double> e_expectations = arma::Col<double>(n_variations);         // Energy expectation values.
    arma::Col<double> alphas = arma::Col<double>(n_variations);                 // Variational parameter.
    arma::Mat<double> qforce_current = arma::Mat<double>(n_dims, n_particles);  // Current quantum force.
    arma::Mat<double> qforce_new = arma::Mat<double>(n_dims, n_particles);      // New quantum force.

    std::mt19937 engine;      // Mersenne Twister RNG.
    std::uniform_real_distribution<double> uniform;  // Continuous uniform distribution.
    std::normal_distribution<double> normal;         // Gaussian distribution

    double (*local_energy_ptr)(arma::Mat<double>, double, double);  // Function pointer.
    double (*wave_function_exponent_ptr)(arma::Mat<double>, double, double);

public:
    VMC()
    {
        // Pre-filling the alphas vector due to parallelization.
        alphas.fill(alpha_step);
        alphas = arma::cumsum(alphas);

        e_expectations.zeros(); // Array must be zeroed since values will be added.
        engine.seed(seed);

        // Determine the dimensionality once instead of checks inside the loops.
        if (n_dims == 1)
        {
            local_energy_ptr = &local_energy_1d;
            wave_function_exponent_ptr = &wave_function_exponent_1d;
        }
        else if (n_dims == 2)
        {
            local_energy_ptr = &local_energy_2d;
            wave_function_exponent_ptr = &wave_function_exponent_2d;
        }
        else if (n_dims == 3)
        {
            local_energy_ptr = &local_energy_3d;
            wave_function_exponent_ptr = &wave_function_exponent_3d;
        }
    }

    void brute_force()
    {   /*
        Brute-force Monte Carlo simulation using Metropolis.
        */
        // Declared outside loop due to parallelization.
        int particle;   // Index for particle loop.
        int _;          // Index for MC loop.
        int dim;        // Index for dimension loop.
        int brute_force_counter = 0; // Debug counter for the Metropolis algorithm.

        for (int i = 0; i < n_variations; i++)
        {   /*
            Run over all variations.
            */
            e_expectation_squared = 0;

            for (particle = 0; particle < n_particles; particle++)
            {   /*
                Iterate over all particles.  In this loop, all current
                positions are calulated along with the current wave
                functions.
                */
                for (dim = 0; dim < n_dims; dim++)
                {
                    pos_current(dim, particle) = step_size*(uniform(engine) - 0.5);
                }
                wave_current(particle) =
                    wave_function_exponent_ptr(
                        pos_current.col(particle),  // Particle position.
                        alphas(i),
                        beta
                    );
            }

            for (_ = 0; _ < n_mc_cycles; _++)
            {   /*
                Run over all Monte Carlo cycles.
                */
                for (particle = 0; particle < n_particles; particle++)
                {   /*
                    Iterate over all particles.  In this loop, new
                    proposed positions and wave functions are
                    calculated.
                    */
                    for (dim = 0; dim < n_dims; dim++)
                    {
                        pos_new(dim, particle) = pos_current(dim, particle) + step_size*(uniform(engine) - 0.5);
                    }
                    wave_new(particle) =
                        wave_function_exponent_ptr(
                            pos_new.col(particle),  // Particle position.
                            alphas(i),
                            beta
                        );

                    exponential_diff =
                        2*(wave_new(particle) - wave_current(particle));

                    if (uniform(engine) < std::exp(exponential_diff))
                    {   /*
                        Perform the Metropolis algorithm.  To save one
                        exponential calculation, the difference is taken
                        of the exponents instead of the ratio of the
                        exponentials. Marginally better...
                        */
                        for (dim = 0; dim < n_dims; dim++)
                        {
                            pos_current(dim, particle) = pos_new(dim, particle);
                        }
                        wave_current(particle) = wave_new(particle);
                        brute_force_counter += 1;   // Debug.
                    }

                    energy_step = local_energy_ptr(
                        pos_current.col(particle),
                        alphas(i),
                        beta
                    );

                    e_expectations(i) += energy_step;
                    e_expectation_squared += energy_step*energy_step;
                }
            }

            e_expectations(i) /= n_mc_cycles;
            e_expectation_squared /= n_mc_cycles;
            e_variances(i) =
                e_expectation_squared - e_expectations(i)*e_expectations(i);
        }
    std::cout << "\nbrute_force: " << brute_force_counter/n_mc_cycles << std::endl;
    }


    void importance_sampling(double t)
    {   /*
        Task 1c importance sampling is implemented here.
        */
        time_step = t;

        //std::cout<<time_step<<std::endl;

        // Declared outside loop due to parallelization.
        int particle;   // Index for particle loop.
        int _;          // Index for MC loop.
        int dim;        // Index for dimension loop.

        int importance_counter = 0;

        for (int i = 0; i < n_variations; i++)
        {   /*
            Run over all variations.
            */
            e_expectation_squared = 0;

            for (particle = 0; particle < n_particles; particle++)
            {   /*
                Iterate over all particles. Set the initial current positions
                calculate the wave function and quantum force.
                */
                for (dim = 0; dim < n_dims; dim++)
                {
                    pos_current(dim, particle) = normal(engine)*sqrt(time_step);

                    qforce_current(dim, particle) =
                        -4*alphas(i)*pos_current(dim, particle);
                }
                wave_current(particle) =
                    wave_function_exponent_ptr(
                        pos_current.col(particle),  // Particle position.
                        alphas(i),
                        beta
                    );
            }

            for (_ = 0; _ < n_mc_cycles; _++)
            {   /* Run over all Monte Carlo cycles. */

                for (particle = 0; particle < n_particles; particle++)
                {   /*
                    Iterate over all particles. Suggest new positions,
                    calculate new wave function and quantum force.
                    TODO: break lines on long expressions.
                    */
                    for (dim = 0; dim < n_dims; dim++)
                    {
                        pos_new(dim, particle) = pos_current(dim, particle) +
                            diffusion_coeff*qforce_current(dim, particle)*time_step +
                            normal(engine)*sqrt(time_step);

                        qforce_new(dim, particle) =
                            -4*alphas(i)*pos_new(dim, particle);
                    }
                    wave_new(particle) =
                        wave_function_exponent_ptr(
                            pos_new.col(particle),  // Particle position.
                            alphas(i),
                            beta
                        );

                    double greens_ratio = 0.0;
                    for (int dim = 0; dim < n_dims; dim++)
                    {   /*
                        Calculate greens ratio for the acceptance
                        criterion.
                        */
                        greens_ratio +=
                            0.5*(qforce_current(dim, particle) + qforce_new(dim, particle))
                            *(0.5*diffusion_coeff*time_step*
                            (qforce_current(dim, particle) - qforce_new(dim, particle))
                            - pos_new(dim, particle) + pos_current(dim, particle));
                    }

                    greens_ratio = exp(greens_ratio);
                    exponential_diff =
                        2*(wave_new(particle) - wave_current(particle));

                    if (uniform(engine) < greens_ratio*std::exp(exponential_diff))
                    {   /*
                        Metropolis step with new acceptance criterion.
                        */
                        for (dim = 0; dim < n_dims; dim++)
                        {
                            pos_current(dim, particle) = pos_new(dim, particle);
                            qforce_current(dim, particle) = qforce_new(dim, particle);
                        }

                        wave_current(particle) = wave_new(particle);
                        importance_counter += 1;    // Debug.
                    }
                    energy_step = local_energy_ptr(
                        pos_current.col(particle),
                        alphas(i),
                        beta
                    );
                    e_expectations(i) += energy_step;
                    e_expectation_squared += energy_step*energy_step;
                }
            }

            e_expectations(i) /= n_mc_cycles;
            e_expectation_squared /= n_mc_cycles;
            e_variances(i) =
            e_expectation_squared - e_expectations(i)*e_expectations(i);
        }
        std::cout << "\nimportance_sampling: " << importance_counter/n_mc_cycles << std::endl;
    }


    void write_to_file(std::string fpath)
    {
        outfile.open(fpath, std::ios::out);
        outfile << std::setw(20) << "alpha";
        outfile << std::setw(20) << "variance_energy";
        outfile << std::setw(21) << "expected_energy\n";

        for (int i = 0; i < n_variations; i++)
        {   /*
            Write data to file.
            */
            outfile << std::setw(20) << std::setprecision(10);
            outfile << alphas(i);
            outfile << std::setw(20) << std::setprecision(10);
            outfile << e_variances(i);
            outfile << std::setw(20) << std::setprecision(10);
            outfile << e_expectations(i) << "\n";
        }
        outfile.close();
    }

    ~VMC()
    {

    }
};


template < typename Type > std::string to_str (const Type & t)
{// formats floating point number
  std::ostringstream os;
  os << t;
  return os.str ();
}


int main()
{
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();

    VMC q;
    q.brute_force();
    q.write_to_file("generated_data/output_bruteforce.txt");

    std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
    std::chrono::duration<double> comp_time = std::chrono::duration_cast<std::chrono::duration<double> >(t2 - t1);

    std::cout << "\ntotal time: " << comp_time.count() << "s" << std::endl;

    std::chrono::steady_clock::time_point t3 = std::chrono::steady_clock::now();

    // -------------------------------
    // Importance sampling starts here

    double dt = 0.4;  // Set the time step 0.4 is good

    VMC q1;
    q1.importance_sampling(dt);
    q1.write_to_file("generated_data/output_importance_"+to_str(dt)+".txt");

    std::chrono::steady_clock::time_point t4 = std::chrono::steady_clock::now();
    std::chrono::duration<double> comp_time1 = std::chrono::duration_cast<std::chrono::duration<double> >(t4 - t3);

    std::cout << "\ntotal time: " << comp_time1.count() << "s" << std::endl;

    return 0;
}

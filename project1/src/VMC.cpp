#include "VMC.h"

VMC::VMC(
    const int n_dims_input,
    const int n_variations_input,
    const int n_mc_cycles_input,
    const int n_particles_input
) : n_dims(n_dims_input),
    n_variations(n_variations_input),
    n_mc_cycles(n_mc_cycles_input),
    n_particles(n_particles_input)

{   /*
    Class constructor.

    Parameters
    ----------
    n_dims_input : constant integer
        The number of spatial dimensions.
    */
    pos_new = arma::Mat<double>(n_dims, n_particles);         // Proposed new position.
    pos_current = arma::Mat<double>(n_dims, n_particles);     // Current position.
    e_variances = arma::Col<double>(n_variations);            // Energy variances.
    e_expectations = arma::Col<double>(n_variations);         // Energy expectation values.
    alphas = arma::Col<double>(n_variations);                 // Variational parameter.
    qforce_current = arma::Mat<double>(n_dims, n_particles);  // Current quantum force.
    qforce_new = arma::Mat<double>(n_dims, n_particles);      // New quantum force.
    test_local = arma::Row<double>(n_mc_cycles);              // Temporary
    energies = arma::Mat<double>(n_mc_cycles, n_variations);

    acceptances = arma::Col<double>(n_variations);   // Debug.
    acceptances.zeros();

    // Pre-filling the alphas vector due to parallelization.
    alphas.fill(alpha_step);
    alphas = arma::cumsum(alphas);
    e_expectations.zeros(); // Array must be zeroed since values will be added.
    energies.zeros();
    engine.seed(seed);

    set_local_energy();
    set_wave_function();
}

void VMC::set_local_energy()
{   /*
    Set pointers to the correct local energy function.
    */
    //std::cout << "VMC.cpp: set_local_energy()" << std::endl;
    if (n_dims == 1)
    {
        local_energy_ptr = &local_energy_1d;
    }
    else if (n_dims == 2)
    {
        local_energy_ptr = &local_energy_2d;
    }
    else if (n_dims == 3)
    {
        local_energy_ptr = &local_energy_3d;
    }
}

void VMC::set_wave_function()
{   /*
    Set pointers to the correct wave function exponent.
    */
    //std::cout << "VMC.cpp: set_wave_function()" << std::endl;
    if (n_dims == 1)
    {
        wave_function_exponent_ptr = &wave_function_exponent_1d;
    }
    else if (n_dims == 2)
    {
        wave_function_exponent_ptr = &wave_function_exponent_2d;
    }
    else if (n_dims == 3)
    {
        wave_function_exponent_ptr = &wave_function_exponent_3d;
    }
}

void VMC::set_initial_positions(int dim, int particle, double alpha)
{   /*
    This function will be overwritten by child class method.
    */
    std::cout << "NotImplementedError" << std::endl;
}

void VMC::set_new_positions(int dim, int particle, double alpha)
{   /*
    This function will be overwritten by child class method.
    */
    std::cout << "NotImplementedError" << std::endl;
}

void VMC::metropolis(int dim, int particle, double alpha, int &acceptance)
{   /*
    This function will be overwritten by child class method.
    */
    std::cout << "NotImplementedError" << std::endl;
}

void VMC::solve()
{   /*
    This function will be overwritten by child class method.  The solve
    method handles the progression of the variational parameter, alpha.
    */
    std::cout << "NotImplementedError" << std::endl;
}

void VMC::one_variation(int variation)
{   /*
    Perform calculations for a single variational parameter.

    Parameters
    ----------
    variation : int
        Which iteration of variational parameter alpha.
    */

    double alpha = alphas(variation);
    int acceptance = 0;  // Debug.

    wave_current = 0;   // Reset wave function for each variation.
    energy_expectation = 0; // Reset for each variation.
    energy_variance = 0; // Reset for each variation.

    energy_expectation_squared = 0;
    for (particle = 0; particle < n_particles; particle++)
    {   /*
        Iterate over all particles.  In this loop, all current
        positions are calulated along with the current wave
        functions.
        */
        for (dim = 0; dim < n_dims; dim++)
        {
            set_initial_positions(dim, particle, alpha);
        }
        wave_current += wave_function_exponent_ptr(
            pos_current.col(particle),  // Particle position.
            alpha,
            beta
        );
    }
    
    #pragma omp parallel for \
        private(mc, particle, dim, particle_inner) \
        firstprivate(wave_new, wave_current, local_energy) \
        firstprivate(pos_new, qforce_new, pos_current, qforce_current) \
        reduction(+:acceptance, energy_expectation, energy_expectation_squared)
    for (mc = 0; mc < n_mc_cycles; mc++)
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
                set_new_positions(dim, particle, alpha);
            }

            wave_new = 0;   // Overwrite the new wave func from previous particle step.
            for (particle_inner = 0; particle_inner < n_particles; particle_inner++)
            {   /*
                After moving one particle, the wave function is
                calculated based on all particle positions.
                */
                wave_new += wave_function_exponent_ptr(
                        pos_new.col(particle_inner),  // Particle position.
                        alpha,
                        beta
                    );
            }
            pos_current(1, 1) = 1337;
            metropolis(dim, particle, alpha, acceptance);
            std::cout << pos_current(1, 1) << std::endl;
            exit(0);

            energy_expectation += local_energy;
            energy_expectation_squared += local_energy*local_energy;
        }
        energies(mc, variation) = local_energy;
    }

    energy_expectation /= n_mc_cycles;
    energy_expectation_squared /= n_mc_cycles;
    energy_variance = energy_expectation_squared
        - energy_expectation*energy_expectation/n_particles;

    acceptances(variation) = acceptance;    // Debug.

    //std::cout << "alpha:    " << alpha  << std::endl;
    //std::cout << "<E^2>:    " << energy_expectation_squared <<std::endl;
    //std::cout << "<E>^2:    " << energy_expectation*energy_expectation << std::endl;
    //std::cout << "sigma^2:  " << energy_variance << std::endl;
    //std::cout << "" << std::endl;

}

void VMC::write_to_file(std::string fpath)
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

void VMC::write_to_file_particles(std::string fpath)
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
        outfile << e_variances(i)/n_particles;
        outfile << std::setw(20) << std::setprecision(10);
        outfile << e_expectations(i)/n_particles << "\n";
    }
    outfile.close();
}

void VMC::write_energies_to_file(std::string fpath)
{
    outfile.open(fpath, std::ios::out);

    for (int i = 0; i < n_variations; i++){
      outfile << std::setw(20) << std::setprecision(10);
      outfile << alphas(i);
    }

    outfile << "\n";
    energies.save(outfile, arma::raw_ascii);
    outfile.close();
}

VMC::~VMC()
{
    // acceptances.print();
}
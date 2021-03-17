#include "wave_function.h"

double wave_function(double x, double y, double z, double alpha, double beta)
{
    return std::exp(-alpha*(x*x + y*y + beta*z*z));
}

double wave_function_exponent_1d_no_interaction(arma::Mat<double> pos, double alpha, double beta)
{   /*
    Parameters
    ----------
    pos : arma::Col<double>
        Vector of position of a particle.

    alpha : double
        Variational parameter.

    beta : double
        ??? parameter.

    Returns
    -------
    : double
        The resulting wave function exponent.
    */
    return -alpha*pos(0)*pos(0);
}

double wave_function_exponent_2d_no_interaction(arma::Mat<double> pos, double alpha, double beta)
{   /*
    Parameters
    ----------
    pos : arma::Col<double>
        Vector of position of a particle.

    alpha : double
        Variational parameter.

    beta : double
        ??? parameter.

    Returns
    -------
    : double
        The resulting wave function exponent.
    */
    return -alpha*(pos(0)*pos(0) + pos(1)*pos(1));
}

double wave_function_exponent_3d_no_interaction(arma::Mat<double> pos, double alpha, double beta)
{   /*
    Parameters
    ----------
    pos : arma::Col<double>
        Vector of position of a particle.

    alpha : double
        Variational parameter.

    beta : double
        ??? parameter.

    Returns
    -------
    : double
        The resulting wave function exponent.
    */
    return -alpha*(pos(0)*pos(0) + pos(1)*pos(1) + beta*pos(2)*pos(2));
}

double wave_function_3d_no_interaction(arma::Mat<double> pos, double alpha, double beta)
{   /*
    Parameters
    ----------
    pos : arma::Col<double>
        Vector of position of a particle.

    alpha : double
        Variational parameter.

    beta : double
        ??? parameter.

    Returns
    -------
    : double
        The resulting wave function evaluated at pos.
    */
    return std::exp(-alpha*(pos(0)*pos(0) + pos(1)*pos(1) + beta*pos(2)*pos(2)));
}

double wave_function_3d_no_interaction_with_loop(
    arma::Mat<double> pos,
    double alpha,
    double beta,
    int n_particles
    )
{   /*
    Parameters
    ----------
    pos : arma::Col<double>
        Vector of position of a particle.

    alpha : double
        Variational parameter.

    beta : double
        ??? parameter.

    Returns
    -------
    : double
        The resulting wave function evaluated at pos.
    */

    double wave_function = 1;
    for (int particle = 0; particle < n_particles; particle++)
    {
        wave_function *= std::exp(-alpha*(
            pos(0, particle)*pos(0, particle) +
            pos(1, particle)*pos(1, particle) +
            pos(2, particle)*pos(2, particle)*beta
        ));
    }

    return wave_function;
}

double wave_function_3d_diff_wrt_alpha(arma::Mat<double> pos, double alpha, double beta)
{   /*
    CORRECTION: This is only the factor in front of the wave function
    after differentiation. 

    Parameters
    ----------
    pos : arma::Col<double>
        Vector of position of a particle.

    alpha : double
        Variational parameter.

    beta : double
        ??? parameter.

    Returns
    -------
    : double
        The wave function differentiated with respect to alpha evaluated
        at pos divided by the wavefunction.
    */
    return -(pos(0)*pos(0) + pos(1)*pos(1) + beta*pos(2)*pos(2));
}

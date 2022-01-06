#include <complex>
#include <type_traits>
#include <iostream>
#include <vector>
#include <queue>
#include <cstdint>
#include <algorithm>
#include <chrono>
#include "lodepng/lodepng.h"

struct sparseSpectralComposition 
{
    std::uint32_t ni, nj;
    std::vector<std::uint32_t> begin_rows;
    std::vector<std::uint32_t> ind_cols;
    std::vector<std::complex<double>> coefficients;
};

std::complex<double>* discretTransformFourier( std::uint32_t width, std::uint32_t height, unsigned char const* pixels )
{
 	
    constexpr const double pi = 3.14159265358979324;
    std::uint32_t ni = height;   //ni nombre de ligne
    std::uint32_t nj = width;    //nj nombre de colonne
    std::complex<double>* X = new std::complex<double>[ni*nj];
    std::fill(X, X+ni*nj, std::complex<double>(0.,0.));
	
    int rank, size;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0)// Si je suis le maître
    {
      // Je realloue pour le maître
      std::complex<double>* Xbis = std::complex<double>[ni*nj];
      
      /* Je distribue les size-1 premières lignes sur les autres processus */
      irow = 0;
      for ( int p = 1; p < size; ++p )
      {
        MPI_Send(&irow, 1, MPI_INT, p, 101, MPI_COMM_WORLD);
        irow ++;
      }
	    
      do// Boucle sur les lignes restantes à distribuer
      {
        /* Puis j'attends le résultat d'un esclave avant de lui envoyer une nouvelle ligne à calculer*/
        MPI_Recv(row.data(), W, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int jrow, from_who;
        from_who = status.MPI_SOURCE;
        jrow     = status.MPI_TAG;
        MPI_Send(&irow, 1, MPI_INT, from_who, 101, MPI_COMM_WORLD);
        std::copy(row.data(), row.data()+W, pixels.data() + W*(H-jrow-1));
        irow++;
      } while (irow < H);
      // On n'a plus de lignes à distribuer. On reçoit les dernières lignes et on signales aux esclaves
      // qu'ils n'ont plus de travail à effectuer
      irow = -1; // -1 => signal de terminaison
      for ( int p = 1; p < size; ++p )
      {
        MPI_Recv(row.data(), W, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int jrow, from_who;
        from_who = status.MPI_SOURCE;
        jrow     = status.MPI_TAG;
        MPI_Send(&irow, 1, MPI_INT, from_who, 101, MPI_COMM_WORLD);
        
      }
    } //puis les esclaves
	else
    { 
        do
        {
          MPI_Recv(&irow, 1, MPI_INT, 0, 101, MPI_COMM_WORLD, &status);
          if (irow != -1)
          {
            for (std::uint32_t k2 = 0; k2 < nj; ++k2)
        {
            for (std::uint32_t n2 = 0; n2 < irow; ++n2 )
            {
                std::complex<double> exp2(std::cos(-2*pi*n2*k2/height), std::sin(-2*pi*n2*k2/height));
                for (std::uint32_t n1 = 0; n1 < nj; ++n1 )
                {
                    std::complex<double> exp1(std::cos(-2*pi*n1*k1/nj), std::sin(-2*pi*n1*k1/nj));
                    X[k1*nj+k2] += double(pixels[3*(n1+n2*nj)])*exp1*exp2;
                }
            }
        }
		  
		
            MPI_Send(row.data(), W, MPI_INT, 0, irow, MPI_COMM_WORLD);
          }
        } while (irow != -1);
    }
    
    for( std::uint32_t k1 = 0; k1 < ni; ++k1 )
    {
        for (std::uint32_t k2 = 0; k2 < nj; ++k2)
        {
            for (std::uint32_t n2 = 0; n2 < ni; ++n2 )
            {
                std::complex<double> exp2(std::cos(-2*pi*n2*k2/height), std::sin(-2*pi*n2*k2/height));
                for (std::uint32_t n1 = 0; n1 < nj; ++n1 )
                {
                    std::complex<double> exp1(std::cos(-2*pi*n1*k1/nj), std::sin(-2*pi*n1*k1/nj));
                    X[k1*nj+k2] += double(pixels[3*(n1+n2*nj)])*exp1*exp2;
                }
            }
        }
    }
    return X;
}

sparseSpectralComposition compressSpectralComposition( std::uint32_t width, std::uint32_t height, const std::complex<double>* plainCoefs, double tauxCompression )
{
    std::uint32_t nb_coefs = std::uint32_t(tauxCompression*width*height);
    struct node 
    {
        node(std::uint32_t t_i, std::uint32_t t_j, double val )
            :   i(t_i),
                j(t_j),
                module(val)
        {}

        bool operator < ( node const& v ) const
        {
            return this->module < v.module;
        }

        bool operator > ( node const& v ) const
        {
            return this->module > v.module;
        }

        bool operator < ( double v ) const
        {
            return this->module < v;
        }

        bool operator > ( double v ) const
        {
            return this->module > v;
        }

        std::uint32_t i,j;
        double module;
    };

    std::priority_queue<node,std::vector<node>,std::greater<node>> queue;
    for ( std::size_t i = 0; i < height; ++i )
        for ( std::size_t j = 0; j < width;  ++j )
        {
            if (queue.size() < nb_coefs)
            {
                queue.emplace(i,j,std::abs(plainCoefs[i*width+j]));
            }
            else
            {
                double val = std::abs(plainCoefs[i*width+j]);
                queue.emplace(i,j,val);
                queue.pop();
            }
        }
    //
    std::vector<std::uint32_t> nbCoefsPerRow(height, 0u);
    auto queue2 = queue;
    while (!queue2.empty())
    {
        std::uint32_t i = queue2.top().i;
        nbCoefsPerRow[i] += 1;
        queue2.pop();       
    }
    //
    sparseSpectralComposition sparse;
    sparse.begin_rows.resize(height+1);
    sparse.begin_rows[0] = 0;
    for (std::uint32_t i = 1; i <= height; ++i )
        sparse.begin_rows[i] = sparse.begin_rows[i-1] + nbCoefsPerRow[i-1];
    std::cout << "Nombre de coefficients de fourier restant : " << sparse.begin_rows[height] << std::endl << std::flush;
    sparse.ind_cols.resize(sparse.begin_rows[height]);
    sparse.coefficients.resize(sparse.begin_rows[height]);
    //
    std::fill(nbCoefsPerRow.begin(), nbCoefsPerRow.end(), 0u);
    //
    while (!queue.empty())
    {
        node n = queue.top();
        queue.pop();
        std::uint32_t ind = sparse.begin_rows[n.i]+nbCoefsPerRow[n.i];
        sparse.ind_cols[ind] = n.j;
        sparse.coefficients[ind] = plainCoefs[n.i*width+n.j];
        nbCoefsPerRow[n.i] += 1;
    }
    sparse.ni = height;
    sparse.nj = width;
    return sparse;
}

unsigned char* inversePartialDiscretTransformFourier( sparseSpectralComposition const& sparse )
{
    constexpr const double pi = 3.14159265358979324;
    unsigned char* x = new unsigned char[3*sparse.ni*sparse.nj];
    std::uint32_t ni = sparse.ni;
    std::uint32_t nj = sparse.nj;
    std::fill(x, x+3*ni*nj, 0);
    for (std::uint32_t n1 = 0; n1 < nj; ++n1 )
    {
        for (std::uint32_t n2 = 0; n2 < ni; ++n2 )
        {
            std::complex<double> val(0.);
            for( std::uint32_t k1 = 0; k1 < ni; ++k1 )
            {
                std::complex<double> exp1(std::cos(+2*pi*n1*k1/ni), std::sin(+2*pi*n1*k1/ni));
                for (std::uint32_t k2 = sparse.begin_rows[k1]; k2 < sparse.begin_rows[k1+1]; ++k2)
                {
                    std::complex<double> exp2(std::cos(+2*pi*n2*sparse.ind_cols[k2]/nj), std::sin(+2*pi*n2*sparse.ind_cols[k2]/nj));
                    val += sparse.coefficients[k2]*exp1*exp2;
                }
            }
            for (std::uint32_t c = 0; c < 3; ++c )
                x[3*(n1+n2*nj)+c] = static_cast<unsigned char>(val.real()/(ni*nj));
        }
    }
    return x;
}

int main(int nargs, char* argv[])
{
    MPI_Init(&nargs, &argv);
    int rank, nbp;

    MPI_Comm_rank(MPI_COMM_WORLD,
                  &rank);
    MPI_Comm_size(MPI_COMM_WORLD, 
                  &nbp);
	
	
	
    std::chrono::time_point<std::chrono::system_clock> start, end,un,deux;

    std::uint32_t width, height;
    unsigned char* image;
    if (nargs <= 1)
    {
        auto error = lodepng_decode24_file(&image, &width, &height, "data/tiny_lena_gray.png");
        if(error) std::cerr << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
    }
    else 
    {
        auto error = lodepng_decode24_file(&image, &width, &height, argv[1]);
        if(error) std::cerr << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
    }
    double taux = 0.10; // A changer si on veut pour une image mieux compressée ou de meilleur qualité
    if (nargs > 2)
        taux = std::stod(argv[2]);
    std::cout << "Caracteristique de l'image : " << width << "x" << height << " => " << width*height << " pixels." << std::endl << std::flush;
	
    start = std::chrono::system_clock::now();                                        // debut de l'horloge
	
    std::complex<double>* fourier = discretTransformFourier( width, height, image );
    un = std::chrono::system_clock::now();                                           // premier arrêt
    std::chrono::duration < double >elapsed_seconds = un - start;
    std::cout << "L’encodage de l’image par DFT prends : " << elapsed_seconds.count() << " secondes\n";

    auto sparse = compressSpectralComposition(width, height, fourier, taux);
    deux = std::chrono::system_clock::now();                                           // deuxième arrêt
    std::chrono::duration < double >elapsed_seconds_1 = deux - un;
    std::cout << "La sélection des p plus gros coeﬀicients prends : " << elapsed_seconds_1.count() << " secondes\n";
    
    unsigned char* compressed_img = inversePartialDiscretTransformFourier(sparse);

    end = std::chrono::system_clock::now();                                           // arrêt finale
    std::chrono::duration < double >elapsed_seconds_2 = end - deux;
    std::cout << "La reconstitution de l’image ”compressée” prends : " << elapsed_seconds_2.count() << " secondes\n";

    auto error = lodepng_encode24_file("compress.png", compressed_img, width, height);
    if(error) std::cerr << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;

    std::cout << "Fin du programme..." << std::endl << std::flush;
    delete [] compressed_img;
    delete [] fourier;
    delete [] image;

    return EXIT_SUCCESS;
}

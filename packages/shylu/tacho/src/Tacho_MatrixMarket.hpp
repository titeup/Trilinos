#ifndef __TACHO_MATRIX_MARKET_HPP__
#define __TACHO_MATRIX_MARKET_HPP__

/// \file Tacho_MatrixMarket.hpp
/// \brief IO utilities for matrix market format
/// \author Kyungjoo Kim (kyukim@sandia.gov)  

#include "Tacho_Util.hpp"

namespace Tacho { 

  class MatrixMarket {
  public:
    /// ------------------------------------------------------------------
    /// Properties: 
    /// - Compile with Device (x), 
    /// - Callable in KokkosFunctors (x), no team interface
    /// - Blocking with fence (x)

    /// \brief elementwise copy 
    template<typename CrsMatrixType>
    static void
    import(CrsMatrixType &A, std::ifstream &file) {
      // static_assert( Kokkos::Impl::is_same<
      //                typename CrsMatrixType::space_type,
      //                Kokkos::HostSpace
      //                >::value,
      //                "Space type of the input should be HostSpace" );

      typedef typename CrsMatrixType::value_type   value_type;
      typedef typename CrsMatrixType::ordinal_type ordinal_type;
      typedef typename CrsMatrixType::size_type    size_type;

      // coordinate format
      typedef Coo<ordinal_type,value_type> ijv_type;

      //typedef typename CrsMatrixType::space_type space_type;
      //typedef Kokkos::RangePolicy<space_type,Kokkos::Schedule<Kokkos::Static> > range_policy_type;
      ordinal_type m, n;
      size_type nnz;

      std::vector<ijv_type> mm;
      const ordinal_type mm_base = 1;
      {
        std::string header;
        if (file.is_open()) {
          std::getline(file, header);
          while (file.good()) {
            char c = file.peek();
            if (c == '%' || c == '\n') {
              file.ignore(256, '\n');
              continue;
            }
            break;
          }
        } else {
          TACHO_TEST_FOR_ABORT(true, MSG_INVALID_INPUT(file));
        }

        // check the header
        bool symmetry = (header.find("symmetric") != std::string::npos);

        // read matrix specification
        file >> m >> n >> nnz;

        mm.reserve(nnz*(symmetry ? 2 : 1));
        for (size_type i=0;i<nnz;++i) {
          ordinal_type row, col;
          value_type val;
          file >> row >> col >> val;

          row -= mm_base;
          col -= mm_base;

          mm.push_back(ijv_type(row, col, val));
          if (symmetry && row != col)
            mm.push_back(ijv_type(col, row, val));
        }

        // sort by row major order
        std::sort(mm.begin(), mm.end(), std::less<ijv_type>());
      }

      // change mm to crs
      std::vector<size_type> ap;
      std::vector<ordinal_type> aj;
      std::vector<value_type> ax;

      ap.reserve(m+1);
      aj.reserve(nnz);
      ax.reserve(nnz);

      {
        ordinal_type ii = 0;
        size_type jj = 0;
        ijv_type prev = mm[0];

        ap.push_back(0);
        aj.push_back(prev.Col());
        ax.push_back(prev.Val());

        for (auto //typename std::vector<ijv_type>::const_iterator 
               it=(mm.begin()+1);it<mm.end();++it) {
          ijv_type aij = (*it);
          
          // row index
          if (aij.Row() != prev.Row()) 
            ap.push_back(aj.size());
          
          if (aij == prev) {
            aj.back()  = aij.Col();
            ax.back() += aij.Val();
          } else {
            aj.push_back(aij.Col());
            ax.push_back(aij.Val());
          }

          prev = aij;
        }
        
        // add the last index to terminate the storage
        ap.push_back(aj.size());
        nnz = aj.size();
      }

      // create crs matrix view
      A.create(m, n, nnz);
      for (ordinal_type i=0;i<m;++i) {
        A.RowPtrBegin(i) = ap.at(i);
        A.RowPtrEnd(i) = ap.at(i+1);
      }
      for (ordinal_type k=0;k<nnz;++k) {
        A.Col(k) = aj.at(k);
        A.Value(k) = ax.at(k);
      }
    }
    
    // int exportMatrixMarket(ofstream &file,
    //                        const string comment,
    //                        const int uplo = 0) {
    //   streamsize prec = file.precision();
    //     file.precision(8);
    //     file << scientific;

    //     file << "%%MatrixMarket matrix coordinate "
    //          << (is_fundamental<value_type>::value ? "real " : "complex ")
    //          << ((uplo == Uplo::Upper || uplo == Uplo::Lower) ? "symmetric " : "general ")
    //          << endl;

    //     file << comment << endl;

    //     // cnt nnz
    //     size_type nnz = 0;
    //     for (ordinal_type i=0;i<_m;++i) {
    //       const size_type jbegin = _ap[i], jend = _ap[i+1];
    //       for (size_type j=jbegin;j<jend;++j) {
    //         if (uplo == Uplo::Upper && i <= _aj[j]) ++nnz;
    //         if (uplo == Uplo::Lower && i >= _aj[j]) ++nnz;
    //         if (!uplo) ++nnz;
    //       }
    //     }
    //     file << _m << " " << _n << " " << nnz << endl;

    //     const int w = 10;
    //     for (ordinal_type i=0;i<_m;++i) {
    //       const size_type jbegin = _ap[i], jend = _ap[i+1];
    //       for (size_type j=jbegin;j<jend;++j) {
    //         bool flag = false;
    //         if (uplo == Uplo::Upper && i <= _aj[j]) flag = true;
    //         if (uplo == Uplo::Lower && i >= _aj[j]) flag = true;
    //         if (!uplo) flag = true;
    //         if (flag) {
    //           value_type val = _ax[j];
    //           file << std::setw(w) << (     i+1) << "  "
    //                << std::setw(w) << (_aj[j]+1) << "  "
    //                << std::setw(w) <<    val << endl;
    //         }
    //       }
    //     }

    //     file.unsetf(ios::scientific);
    //     file.precision(prec);

    //     return 0;
    //   }
  };

}

#endif

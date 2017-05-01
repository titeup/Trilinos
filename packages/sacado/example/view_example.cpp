// @HEADER
// ***********************************************************************
//
//                           Stokhos Package
//                 Copyright (2009) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Eric T. Phipps (etphipp@sandia.gov).
//
// ***********************************************************************
// @HEADER

#include "Sacado.hpp"

//
// A simple example showing how to use Sacado with Kokkos
//

// This code uses lambda's which only work on more recent versions of Cuda
// with a special command-line argument
#if !defined(__CUDACC__)

// This code relies on the Kokkos view specializations being enabled, which
// is the default, but can be disabled by the user
#if defined(HAVE_SACADO_VIEW_SPEC) && !defined(SACADO_DISABLE_FAD_VIEW_SPEC)

// Function to compute matrix-vector product c = A*b using Kokkos
template <typename ViewTypeA, typename ViewTypeB, typename ViewTypeC>
void run_mat_vec(const ViewTypeA& A, const ViewTypeB& b, const ViewTypeC& c) {
  typedef typename ViewTypeC::value_type scalar_type;
  typedef typename ViewTypeC::execution_space execution_space;

  const size_t m = A.dimension_0();
  const size_t n = A.dimension_1();
  Kokkos::parallel_for(
    Kokkos::RangePolicy<execution_space>( 0,m ),
    KOKKOS_LAMBDA (const size_t i) {
      scalar_type t = 0.0;
      for (size_t j=0; j<n; ++j)
        t += A(i,j)*b(j);
      c(i) = t;
    }
  );
}

// Function to compute the derivative of the matrix-vector product
// c = A*b using Kokkos.
template <typename ViewTypeA, typename ViewTypeB, typename ViewTypeC>
void run_mat_vec_deriv(const ViewTypeA& A, const ViewTypeB& b,
                       const ViewTypeC& c) {
  typedef typename ViewTypeC::value_type scalar_type;
  typedef typename ViewTypeC::execution_space execution_space;

  const size_t m = A.dimension_0();
  const size_t n = A.dimension_1();
  const size_t p = A.dimension_2()-1;
  Kokkos::parallel_for(
    Kokkos::RangePolicy<execution_space>( 0,m ),
    KOKKOS_LAMBDA (const int i) {
      // Derivative portion
      for (size_t k=0; k<p; ++k) {
        scalar_type t = 0.0;
        for (size_t j=0; j<n; ++j)
          t += A(i,j,k)*b(j,p) + A(i,j,p)*b(j,k);
        c(i,k) = t;
      }

      // Value portion
      scalar_type t = 0.0;
      for (size_t j=0; j<n; ++j)
        t += A(i,j,p)*b(j,p);
      c(i,p) = t;
    }
  );
}

#endif

#endif

int main(int argc, char* argv[]) {
  int ret = 0;

#if !defined(__CUDACC__)
#if defined(HAVE_SACADO_VIEW_SPEC) && !defined(SACADO_DISABLE_FAD_VIEW_SPEC)

  Kokkos::initialize(argc, argv);
  {

    const size_t m = 10; // Number of rows
    const size_t n = 2;  // Number of columns
    const size_t p = 1;  // Derivative dimension

    // Allocate Kokkos view's for matrix, input vector, and output vector
    // Derivative dimension must be specified as the last constructor argument
    typedef Sacado::Fad::DFad<double> FadType;
    Kokkos::View<FadType**> A("A",m,n,p+1);
    Kokkos::View<FadType*>  b("b",n,p+1);
    Kokkos::View<FadType*>  c("c",m,p+1);

    // Initialize values
    Kokkos::deep_copy( A, FadType(p, 0, 2.0) );
    Kokkos::deep_copy( b, FadType(p, 0, 3.0) );
    Kokkos::deep_copy( c, 0.0 );

    // Run mat-vec
    run_mat_vec(A,b,c);

    // Print result
    std::cout << "\nc = A*b:  Differentiated using Sacado:" << std::endl;
    for (size_t i=0; i<m; ++i)
      std::cout << "\tc(" << i << ") = " << c(i) << std::endl;

    // Now compute derivative analytically.  Any Sacado view can be flattened
    // into a standard view of one higher rank, with the extra dimension equal
    // to p+1
    Kokkos::View<FadType*> c2("c",m,p+1);
    Kokkos::View<double***> A_flat = A;
    Kokkos::View<double**>  b_flat = b;
    Kokkos::View<double**>  c_flat = c2;
    run_mat_vec_deriv(A_flat, b_flat, c_flat);

    // Print result
    std::cout << "\nc = A*b:  Differentiated analytically:" << std::endl;
    for (size_t i=0; i<m; ++i)
      std::cout << "\tc(" << i << ") = " << c2(i) << std::endl;

    // Compute the error
    double err = 0.0;
    for (size_t i=0; i<m; ++i) {
      for (size_t k=0; k<p; ++k)
        err += std::abs(c(i).fastAccessDx(k)-c2(i).fastAccessDx(k));
      err += std::abs(c(i).val()-c2(i).val());
    }

    double tol = 1.0e-14;
    if (err < tol) {
      ret = 0;
      std::cout << "\nExample passed!" << std::endl;
    }
    else {
      ret = 1;
      std::cout <<"\nSomething is wrong, example failed!" << std::endl;
    }

  }
  Kokkos::finalize();

#endif
#endif

  return ret;
}

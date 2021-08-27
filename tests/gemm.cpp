#include <experimental/linalg>
#include <experimental/mdspan>

// FIXME I can't actually test the executor overloads, since my GCC
// (9.1.0, via Homebrew) isn't set up correctly:
//
// .../gcc/9.1.0/include/c++/9.1.0/pstl/parallel_backend_tbb.h:19:10: fatal error: tbb/blocked_range.h: No such file or directory
//   19 | #include <tbb/blocked_range.h>
//      |          ^~~~~~~~~~~~~~~~~~~~~

//#include <execution>
#include <vector>
#include "gtest/gtest.h"
#include <iostream>

namespace {
  using std::experimental::mdspan;
  using std::experimental::dynamic_extent;
  using std::experimental::extents;
  using std::experimental::layout_left;
  using std::experimental::linalg::explicit_diagonal;
  using std::experimental::linalg::implicit_unit_diagonal;
  using std::experimental::linalg::lower_triangle;
  using std::experimental::linalg::matrix_product;
  using std::experimental::linalg::transposed;
  using std::experimental::linalg::upper_triangle;
  using std::cout;
  using std::endl;

  template<class MdspanType, class Scalar>
  struct FillMatrix {
    static void fill(MdspanType A, const Scalar startVal);
  };

  template<class MdspanType>
  struct FillMatrix<MdspanType, int> {
    static void fill(MdspanType A, const int startVal)
    {
      const ptrdiff_t A_numRows = A.extent(0);
      const ptrdiff_t A_numCols = A.extent(1);
      for (ptrdiff_t j = 0; j < A_numCols; ++j) {
        for (ptrdiff_t i = 0; i < A_numRows; ++i) {
          A(i,j) = int((i+startVal) + (j+startVal) * A_numRows);
        }
      }
    }
  };

  template<class MdspanType>
  struct FillMatrix<MdspanType, double> {
    static void fill(MdspanType A, const double startVal)
    {
      const ptrdiff_t A_numRows = A.extent(0);
      const ptrdiff_t A_numCols = A.extent(1);
      for (ptrdiff_t j = 0; j < A_numCols; ++j) {
        for (ptrdiff_t i = 0; i < A_numRows; ++i) {
          A(i,j) = (double(i)+startVal) +
            (double(j)+startVal) * double(A_numRows);
        }
      }
    }
  };

  template<class MdspanType, class Scalar>
  void fill_matrix(MdspanType A, const Scalar startVal) {
    FillMatrix<MdspanType, Scalar>::fill(A, startVal);
  }

  template<class Scalar>
  struct Magnitude {
    using type = Scalar;
  };
  template<class Real>
  struct Magnitude<std::complex<Real>> {
    using type = Real;
  };

  template<class Scalar>
  void test_matrix_product()
  {
    using scalar_t = Scalar;
    using real_t = typename Magnitude<Scalar>::type;

    using extents_t = extents<dynamic_extent, dynamic_extent>;
    using matrix_t = mdspan<scalar_t, extents_t, layout_left>;

    constexpr ptrdiff_t maxDim = 7;
    constexpr ptrdiff_t storageSize(7*maxDim*maxDim);
    std::vector<scalar_t> storage(storageSize);

    for (ptrdiff_t C_numRows : {1, 4, 7}) {
      for (ptrdiff_t C_numCols : {1, 4, 7}) {
        for (ptrdiff_t A_numCols : {1, 4, 7}) {

          const ptrdiff_t A_numRows = C_numRows;
          const ptrdiff_t B_numRows = A_numCols;
          const ptrdiff_t B_numCols = C_numCols;

          ptrdiff_t offset = 0;
          matrix_t A(storage.data() + offset, A_numRows, A_numCols);
          offset += A_numRows * A_numCols;
          matrix_t B(storage.data() + offset, B_numRows, B_numCols);
          offset += B_numRows * B_numCols;
          matrix_t C(storage.data() + offset, C_numRows, C_numCols);
          offset += C_numRows * C_numCols;
          matrix_t C2(storage.data() + offset, C_numRows, C_numCols);
          offset += C_numRows * C_numCols;
          matrix_t A_t(storage.data() + offset, A_numCols, A_numRows);
          offset += A_numCols * A_numRows;
          matrix_t B_t(storage.data() + offset, B_numCols, B_numRows);
          offset += B_numCols * B_numRows;
          matrix_t C3(storage.data() + offset, C_numRows, C_numCols);
          offset += C_numRows * C_numCols;

          fill_matrix(A, scalar_t(real_t(1)));
          fill_matrix(B, scalar_t(real_t(2)));
          for (ptrdiff_t j = 0; j < A_numCols; ++j) {
            for (ptrdiff_t i = 0; i < A_numRows; ++i) {
              A_t(j,i) = A(i,j);
            }
          }
          for (ptrdiff_t j = 0; j < B_numCols; ++j) {
            for (ptrdiff_t i = 0; i < B_numRows; ++i) {
              B_t(j,i) = B(i,j);
            }
          }

          for (ptrdiff_t j = 0; j < C_numCols; ++j) {
            for (ptrdiff_t i = 0; i < C_numRows; ++i) {
              C(i,j) = scalar_t(0.0); // this works even for complex
              for (ptrdiff_t k = 0; k < A_numCols; ++k) {
                C(i,j) += A(i,k) * B(k,j);
              }
            }
          }

          // Fill result matrix with flag values to make sure that we
          // computed everything.
          for (ptrdiff_t j = 0; j < C_numCols; ++j) {
            for (ptrdiff_t i = 0; i < C_numRows; ++i) {
              C2(i,j) = std::numeric_limits<scalar_t>::min();
            }
          }

          cout << " Test C2(" << C_numRows << " x " << C_numCols
               << ") = A(" << A_numRows << " x " << A_numCols
               << ") * B(" << B_numRows << " x " << B_numCols
               << ")" << endl;
          matrix_product(A, B, C2);
          for (ptrdiff_t j = 0; j < C_numCols; ++j) {
            for (ptrdiff_t i = 0; i < C_numRows; ++i) {
              EXPECT_DOUBLE_EQ(C2(i,j), C(i,j)) << "Matrices differ at index (" 
                  << i << "," << j << ")=\n";
            }
          }

          // Fill result matrix with flag values to make sure that
          // we computed everything.
          for (ptrdiff_t j = 0; j < C_numCols; ++j) {
            for (ptrdiff_t i = 0; i < C_numRows; ++i) {
              C3(i,j) = std::numeric_limits<scalar_t>::min();
            }
          }

          cout << " Test C3(" << C3.extent(0) << " x " << C3.extent(1)
               << ") = "
               << "A_t(" << A_t.extent(0) << " x " << A_t.extent(1)
               << ")^T * "
               << "B_t(" << B_t.extent(0) << " x " << B_t.extent(1)
               << ")^T" << endl;
          matrix_product(transposed(A_t),
                         transposed(B_t), C3);
          for (ptrdiff_t j = 0; j < C_numCols; ++j) {
            for (ptrdiff_t i = 0; i < C_numRows; ++i) {
              EXPECT_DOUBLE_EQ(C3(i,j), C(i,j)) << "Matrices differ at index (" 
                  << i << "," << j << ")=\n";
            }
          }

          {
            auto A_tt = transposed(A_t);
            auto B_tt = transposed(B_t);
            EXPECT_EQ(A_tt.extent(0), A.extent(0));
            EXPECT_EQ(A_t.extent(0), A.extent(1));
            EXPECT_EQ(A_t.extent(1), A.extent(0));
            EXPECT_EQ(B_tt.extent(0), B.extent(0));
            EXPECT_EQ(B_t.extent(0), B.extent(1));
            EXPECT_EQ(B_t.extent(1), B.extent(0));
            EXPECT_EQ(A_tt.extent(1), B_tt.extent(0));

            for (ptrdiff_t j = 0; j < C_numCols; ++j) {
              for (ptrdiff_t i = 0; i < C_numRows; ++i) {
                C(i,j) = scalar_t(0.0); // this works even for complex
                for (extents<>::size_type k = 0; k < A_tt.extent(1); ++k) {
                  C(i,j) += A_tt(i,k) * B_tt(k,j);
                }
              }
            }
          }

          cout << " Compare using hand-rolled transposed loop"
               << endl;

          for (ptrdiff_t j = 0; j < C_numCols; ++j) {
            for (ptrdiff_t i = 0; i < C_numRows; ++i) {
              EXPECT_DOUBLE_EQ(C3(i,j), C(i,j)) << "Matrices differ at index (" 
                << i << "," << j << ")=\n";
            }
          }

          cout << " Test C3(" << C3.extent(0) << " x " << C3.extent(1)
               << ") = "
               << "2*A_t(" << A_t.extent(0) << " x " << A_t.extent(1)
               << ")^T * "
               << "B_t(" << B_t.extent(0) << " x " << B_t.extent(1)
               << ")^T" << endl;
          matrix_product(scaled(scalar_t(2.0), transposed(A_t)),
                         transposed(B_t), C3);

          for (ptrdiff_t j = 0; j < C_numCols; ++j) {
            for (ptrdiff_t i = 0; i < C_numRows; ++i) {
              EXPECT_DOUBLE_EQ(C3(i,j), scalar_t(2.0)*C(i,j)) 
                << "Matrices differ at index (" 
                << i << "," << j << ")=\n";
            }
          }
        } // A_numCols
      } // C_numCols
    } // C_numRows
  }

  // Testing int is a way to test the non-BLAS-library implementation.
  TEST(BLAS3_gemm, mdspan_int)
  {
    test_matrix_product<int>();
  }

  TEST(BLAS3_gemm, mdspan_double)
  {
    test_matrix_product<double>();
  }

  TEST(BLAS3_trmm, left_lower_tri_explicit_diag)
  {
    /* C = A * B, where A is triangular mxm */
    using extents_t = extents<dynamic_extent, dynamic_extent>;
    using matrix_t = mdspan<double, extents_t, layout_left>;
    double snan = std::numeric_limits<double>::signaling_NaN();

    int m = 3, n = 2;
    std::vector<double> A_mem(m*m, snan);
    std::vector<double> B_mem(m*n);
    std::vector<double> C_mem(m*n, snan);
    std::vector<double> gs_mem(m*n);

    matrix_t A(A_mem.data(), m, m);
    matrix_t B(B_mem.data(), m, n);
    matrix_t C(C_mem.data(), m, n);
    matrix_t gs(gs_mem.data(), m, n);

    // Fill A
    A(0,0) = 3.5;
    A(1,0) = -2.0;
    A(1,1) = 1.2;
    A(2,0) = -0.1;
    A(2,1) = 4.5;
    A(2,2) = -1.0;
    
    // Fill B
    B(0,0) = -4.4;
    B(0,1) = 1.8;
    B(1,0) = -1.4;
    B(1,1) = 3.4;
    B(2,0) = 1.8;
    B(2,1) = 1.6;

    // Fill GS
    gs(0,0) = -15.4;
    gs(0,1) = 6.3;
    gs(1,0) = 7.12;
    gs(1,1) = 0.48;
    gs(2,0) = -7.66;
    gs(2,1) = 13.52;

    // Check the non-overwriting version
    triangular_matrix_left_product(A, lower_triangle, explicit_diagonal, B, C);

    for (ptrdiff_t j = 0; j < n; ++j) {
      for (ptrdiff_t i = 0; i < m; ++i) {
        EXPECT_DOUBLE_EQ(gs(i,j), C(i,j)) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }

    // Check the overwriting version
    triangular_matrix_left_product(A, lower_triangle, explicit_diagonal, B);

    for (ptrdiff_t j = 0; j < n; ++j) {
      for (ptrdiff_t i = 0; i < m; ++i) {
        EXPECT_DOUBLE_EQ(gs(i,j), B(i,j)) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }
  }

TEST(BLAS3_trmm, left_lower_tri_implicit_diag)
  {
    /* C = A * B, where A is triangular mxm */
    using extents_t = extents<dynamic_extent, dynamic_extent>;
    using matrix_t = mdspan<double, extents_t, layout_left>;
    double snan = std::numeric_limits<double>::signaling_NaN();

    int m = 3, n = 2;
    std::vector<double> A_mem(m*m, snan);
    std::vector<double> B_mem(m*n);
    std::vector<double> C_mem(m*n, snan);
    std::vector<double> gs_mem(m*n);

    matrix_t A(A_mem.data(), m, m);
    matrix_t B(B_mem.data(), m, n);
    matrix_t C(C_mem.data(), m, n);
    matrix_t gs(gs_mem.data(), m, n);

    // Fill A
    A(1,0) = -2.0;
    A(2,0) = -0.1;
    A(2,1) = 4.5;
    
    // Fill B
    B(0,0) = -4.4;
    B(0,1) = 1.8;
    B(1,0) = -1.4;
    B(1,1) = 3.4;
    B(2,0) = 1.8;
    B(2,1) = 1.6;

    triangular_matrix_left_product(A, lower_triangle, implicit_unit_diagonal, B, C);

    // Fill GS
    gs(0,0) = -4.4;
    gs(0,1) = 1.8;
    gs(1,0) = 7.4;
    gs(1,1) = -0.2;
    gs(2,0) = -4.06;
    gs(2,1) = 16.72;

    for (ptrdiff_t j = 0; j < n; ++j) {
      for (ptrdiff_t i = 0; i < m; ++i) {
        // FIXME: Choose a more reasonable value for the tolerance
        double tol = 1e-9;
        EXPECT_NEAR(gs(i,j), C(i,j), tol) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }

    // Check the overwriting version
    triangular_matrix_left_product(A, lower_triangle, implicit_unit_diagonal, B);

    for (ptrdiff_t j = 0; j < n; ++j) {
      for (ptrdiff_t i = 0; i < m; ++i) {
        // FIXME: Choose a more reasonable value for the tolerance
        double tol = 1e-9;
        EXPECT_NEAR(gs(i,j), B(i,j), tol) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }
  }

TEST(BLAS3_trmm, left_upper_tri_explicit_diag)
  {
    /* C = A * B, where A is triangular mxm */
    using extents_t = extents<dynamic_extent, dynamic_extent>;
    using matrix_t = mdspan<double, extents_t, layout_left>;
    double snan = std::numeric_limits<double>::signaling_NaN();

    int m = 3, n = 2;
    std::vector<double> A_mem(m*m, snan);
    std::vector<double> B_mem(m*n);
    std::vector<double> C_mem(m*n, snan);
    std::vector<double> gs_mem(m*n);

    matrix_t A(A_mem.data(), m, m);
    matrix_t B(B_mem.data(), m, n);
    matrix_t C(C_mem.data(), m, n);
    matrix_t gs(gs_mem.data(), m, n);

    // Fill A
    A(0,0) = 3.5;
    A(0,1) = -2.0;
    A(1,1) = 1.2;
    A(0,2) = -0.1;
    A(1,2) = 4.5;
    A(2,2) = -1.0;
    
    // Fill B
    B(0,0) = -4.4;
    B(0,1) = 1.8;
    B(1,0) = -1.4;
    B(1,1) = 3.4;
    B(2,0) = 1.8;
    B(2,1) = 1.6;

    // Fill GS
    gs(0,0) = -12.78;
    gs(0,1) = -0.66;
    gs(1,0) = 6.42;
    gs(1,1) = 11.28;
    gs(2,0) = -1.8;
    gs(2,1) = -1.6;

    // Check the non-overwriting version
    triangular_matrix_left_product(A, upper_triangle, explicit_diagonal, B, C);

    for (ptrdiff_t j = 0; j < n; ++j) {
      for (ptrdiff_t i = 0; i < m; ++i) {
        EXPECT_DOUBLE_EQ(gs(i,j), C(i,j)) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }

    // Check the overwriting version
    triangular_matrix_left_product(A, upper_triangle, explicit_diagonal, B);

    for (ptrdiff_t j = 0; j < n; ++j) {
      for (ptrdiff_t i = 0; i < m; ++i) {
        EXPECT_DOUBLE_EQ(gs(i,j), B(i,j)) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }
  }

TEST(BLAS3_trmm, left_upper_tri_implicit_diag)
  {
    /* C = A * B, where A is triangular mxm */
    using extents_t = extents<dynamic_extent, dynamic_extent>;
    using matrix_t = mdspan<double, extents_t, layout_left>;
    double snan = std::numeric_limits<double>::signaling_NaN();

    int m = 3, n = 2;
    std::vector<double> A_mem(m*m, snan);
    std::vector<double> B_mem(m*n);
    std::vector<double> C_mem(m*n, snan);
    std::vector<double> gs_mem(m*n);

    matrix_t A(A_mem.data(), m, m);
    matrix_t B(B_mem.data(), m, n);
    matrix_t C(C_mem.data(), m, n);
    matrix_t gs(gs_mem.data(), m, n);

    // Fill A
    A(0,1) = -2.0;
    A(0,2) = -0.1;
    A(1,2) = 4.5;
    
    // Fill B
    B(0,0) = -4.4;
    B(0,1) = 1.8;
    B(1,0) = -1.4;
    B(1,1) = 3.4;
    B(2,0) = 1.8;
    B(2,1) = 1.6;

    triangular_matrix_left_product(A, upper_triangle, implicit_unit_diagonal, B, C);

    // Fill GS
    gs(0,0) = -1.78;
    gs(0,1) = -5.16;
    gs(1,0) = 6.7;
    gs(1,1) = 10.6;
    gs(2,0) = 1.8;
    gs(2,1) = 1.6;

    for (ptrdiff_t j = 0; j < n; ++j) {
      for (ptrdiff_t i = 0; i < m; ++i) {
        // FIXME: Choose a more reasonable value for the tolerance
        double tol = 1e-9;
        EXPECT_NEAR(gs(i,j), C(i,j), tol) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }

    // Check the overwriting version
    triangular_matrix_left_product(A, upper_triangle, implicit_unit_diagonal, B);

    for (ptrdiff_t j = 0; j < n; ++j) {
      for (ptrdiff_t i = 0; i < m; ++i) {
        // FIXME: Choose a more reasonable value for the tolerance
        double tol = 1e-9;
        EXPECT_NEAR(gs(i,j), B(i,j), tol) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }
  }

TEST(BLAS3_trmm, right_lower_tri_explicit_diag)
  {
    /* C = B * A, where A is triangular mxm */
    using extents_t = extents<dynamic_extent, dynamic_extent>;
    using matrix_t = mdspan<double, extents_t, layout_left>;
    double snan = std::numeric_limits<double>::signaling_NaN();

    int m = 3, n = 2;
    std::vector<double> A_mem(m*m, snan);
    std::vector<double> B_mem(m*n);
    std::vector<double> C_mem(m*n, snan);
    std::vector<double> gs_mem(m*n);

    matrix_t A(A_mem.data(), m, m);
    matrix_t B(B_mem.data(), n, m);
    matrix_t C(C_mem.data(), n, m);
    matrix_t gs(gs_mem.data(), n, m);

    // Fill A
    A(0,0) = 3.5;
    A(1,0) = -2.0;
    A(1,1) = 1.2;
    A(2,0) = -0.1;
    A(2,1) = 4.5;
    A(2,2) = -1.0;
    
    // Fill B
    B(0,0) = -4.4;
    B(1,0) = 1.8;
    B(0,1) = -1.4;
    B(1,1) = 3.4;
    B(0,2) = 1.8;
    B(1,2) = 1.6;

    // Fill GS
    gs(0,0) = -12.78;
    gs(1,0) = -0.66;
    gs(0,1) = 6.42;
    gs(1,1) = 11.28;
    gs(0,2) = -1.8;
    gs(1,2) = -1.6;

    // Check the non-overwriting version
    triangular_matrix_right_product(A, lower_triangle, explicit_diagonal, B, C);

    for (ptrdiff_t j = 0; j < m; ++j) {
      for (ptrdiff_t i = 0; i < n; ++i) {
        EXPECT_DOUBLE_EQ(gs(i,j), C(i,j)) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }

    // Check the overwriting version
    triangular_matrix_right_product(A, lower_triangle, explicit_diagonal, B);

    for (ptrdiff_t j = 0; j < m; ++j) {
      for (ptrdiff_t i = 0; i < n; ++i) {
        EXPECT_DOUBLE_EQ(gs(i,j), B(i,j)) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }
  }

TEST(BLAS3_trmm, right_lower_tri_implicit_diag)
  {
    /* C = A * B, where A is triangular mxm */
    using extents_t = extents<dynamic_extent, dynamic_extent>;
    using matrix_t = mdspan<double, extents_t, layout_left>;
    double snan = std::numeric_limits<double>::signaling_NaN();

    int m = 3, n = 2;
    std::vector<double> A_mem(m*m, snan);
    std::vector<double> B_mem(m*n);
    std::vector<double> C_mem(m*n, snan);
    std::vector<double> gs_mem(m*n);

    matrix_t A(A_mem.data(), m, m);
    matrix_t B(B_mem.data(), n, m);
    matrix_t C(C_mem.data(), n, m);
    matrix_t gs(gs_mem.data(), n, m);

    // Fill A
    A(1,0) = -2.0;
    A(2,0) = -0.1;
    A(2,1) = 4.5;
    
    // Fill B
    B(0,0) = -4.4;
    B(1,0) = 1.8;
    B(0,1) = -1.4;
    B(1,1) = 3.4;
    B(0,2) = 1.8;
    B(1,2) = 1.6;

    triangular_matrix_right_product(A, lower_triangle, implicit_unit_diagonal, B, C);

    // Fill GS
    gs(0,0) = -1.78;
    gs(1,0) = -5.16;
    gs(0,1) = 6.7;
    gs(1,1) = 10.6;
    gs(0,2) = 1.8;
    gs(1,2) = 1.6;

    for (ptrdiff_t j = 0; j < m; ++j) {
      for (ptrdiff_t i = 0; i < n; ++i) {
        // FIXME: Choose a more reasonable value for the tolerance
        double tol = 1e-9;
        EXPECT_NEAR(gs(i,j), C(i,j), tol) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }

    // Check the overwriting version
    triangular_matrix_right_product(A, lower_triangle, implicit_unit_diagonal, B);

    for (ptrdiff_t j = 0; j < m; ++j) {
      for (ptrdiff_t i = 0; i < n; ++i) {
        // FIXME: Choose a more reasonable value for the tolerance
        double tol = 1e-9;
        EXPECT_NEAR(gs(i,j), B(i,j), tol) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }
  }

TEST(BLAS3_trmm, right_upper_tri_explicit_diag)
  {
    /* C = B*A, where A is triangular mxm */
    using extents_t = extents<dynamic_extent, dynamic_extent>;
    using matrix_t = mdspan<double, extents_t, layout_left>;
    double snan = std::numeric_limits<double>::signaling_NaN();

    int m = 3, n = 2;
    std::vector<double> A_mem(m*m, snan);
    std::vector<double> B_mem(m*n);
    std::vector<double> C_mem(m*n, snan);
    std::vector<double> gs_mem(m*n);

    matrix_t A(A_mem.data(), m, m);
    matrix_t B(B_mem.data(), n, m);
    matrix_t C(C_mem.data(), n, m);
    matrix_t gs(gs_mem.data(), n, m);

    // Fill A
    A(0,0) = 3.5;
    A(0,1) = -2.0;
    A(1,1) = 1.2;
    A(0,2) = -0.1;
    A(1,2) = 4.5;
    A(2,2) = -1.0;
    
    // Fill B
    B(0,0) = -4.4;
    B(1,0) = 1.8;
    B(0,1) = -1.4;
    B(1,1) = 3.4;
    B(0,2) = 1.8;
    B(1,2) = 1.6;

    // Fill GS
    gs(0,0) = -15.4;
    gs(1,0) = 6.3;
    gs(0,1) = 7.12;
    gs(1,1) = 0.48;
    gs(0,2) = -7.66;
    gs(1,2) = 13.52;

    // Check the non-overwriting version
    triangular_matrix_right_product(A, upper_triangle, explicit_diagonal, B, C);

    for (ptrdiff_t j = 0; j < m; ++j) {
      for (ptrdiff_t i = 0; i < n; ++i) {
        EXPECT_DOUBLE_EQ(gs(i,j), C(i,j)) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }

    // Check the overwriting version
    triangular_matrix_right_product(A, upper_triangle, explicit_diagonal, B);

    for (ptrdiff_t j = 0; j < m; ++j) {
      for (ptrdiff_t i = 0; i < n; ++i) {
        EXPECT_DOUBLE_EQ(gs(i,j), B(i,j)) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }
  }

TEST(BLAS3_trmm, right_upper_tri_implicit_diag)
  {
    /* C = B * A, where A is triangular mxm */
    using extents_t = extents<dynamic_extent, dynamic_extent>;
    using matrix_t = mdspan<double, extents_t, layout_left>;
    double snan = std::numeric_limits<double>::signaling_NaN();

    int m = 3, n = 2;
    std::vector<double> A_mem(m*m, snan);
    std::vector<double> B_mem(m*n);
    std::vector<double> C_mem(m*n, snan);
    std::vector<double> gs_mem(m*n);

    matrix_t A(A_mem.data(), m, m);
    matrix_t B(B_mem.data(), n, m);
    matrix_t C(C_mem.data(), n, m);
    matrix_t gs(gs_mem.data(), n, m);

    // Fill A
    A(0,1) = -2.0;
    A(0,2) = -0.1;
    A(1,2) = 4.5;
    
    // Fill B
    B(0,0) = -4.4;
    B(1,0) = 1.8;
    B(0,1) = -1.4;
    B(1,1) = 3.4;
    B(0,2) = 1.8;
    B(1,2) = 1.6;

    triangular_matrix_right_product(A, upper_triangle, implicit_unit_diagonal, B, C);

    // Fill GS
    gs(0,0) = -4.4;
    gs(1,0) = 1.8;
    gs(0,1) = 7.4;
    gs(1,1) = -0.2;
    gs(0,2) = -4.06;
    gs(1,2) = 16.72;

    for (ptrdiff_t j = 0; j < m; ++j) {
      for (ptrdiff_t i = 0; i < n; ++i) {
        // FIXME: Choose a more reasonable value for the tolerance
        double tol = 1e-9;
        EXPECT_NEAR(gs(i,j), C(i,j), tol) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }

    // Check the overwriting version
    triangular_matrix_right_product(A, upper_triangle, implicit_unit_diagonal, B);

    for (ptrdiff_t j = 0; j < m; ++j) {
      for (ptrdiff_t i = 0; i < n; ++i) {
        // FIXME: Choose a more reasonable value for the tolerance
        double tol = 1e-9;
        EXPECT_NEAR(gs(i,j), B(i,j), tol) 
          << "Matrices differ at index (" 
          << i << "," << j << ")\n";
      }
    }
  }

} // end anonymous namespace
/* ************************************************************************
 * Copyright 2016-2020 Advanced Micro Devices, Inc.
 *
 * ************************************************************************ */

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <vector>

#include "cblas_interface.h"
#include "flops.h"
#include "hipblas.hpp"
#include "norm.h"
#include "unit.h"
#include "utility.h"

using namespace std;

/* ============================================================================================ */

template <typename T>
hipblasStatus_t testing_syrk(Arguments argus)
{
    bool FORTRAN       = argus.fortran;
    auto hipblasSyrkFn = FORTRAN ? hipblasSyrk<T, true> : hipblasSyrk<T, false>;

    int N   = argus.N;
    int K   = argus.K;
    int lda = argus.lda;
    int ldc = argus.ldc;

    hipblasFillMode_t  uplo   = char2hipblas_fill(argus.uplo_option);
    hipblasOperation_t transA = char2hipblas_operation(argus.transA_option);

    hipblasStatus_t status = HIPBLAS_STATUS_SUCCESS;

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    if(N < 0 || K < 0 || ldc < N || (transA == HIPBLAS_OP_N && lda < N)
       || (transA != HIPBLAS_OP_N && lda < K))
    {
        return HIPBLAS_STATUS_INVALID_VALUE;
    }

    int K1     = (transA == HIPBLAS_OP_N ? K : N);
    int A_size = lda * K1;
    int C_size = ldc * N;

    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    host_vector<T> hA(A_size);
    host_vector<T> hC(C_size);
    host_vector<T> hC2(C_size);

    device_vector<T> dA(A_size);
    device_vector<T> dC(C_size);

    double gpu_time_used, cpu_time_used;
    double hipblasGflops, cblas_gflops, hipblasBandwidth;
    double rocblas_error;

    T alpha = argus.get_alpha<T>();
    T beta  = argus.get_beta<T>();

    hipblasHandle_t handle;
    hipblasCreate(&handle);

    // Initial Data on CPU
    srand(1);
    hipblas_init<T>(hA, N, K1, lda);
    hipblas_init<T>(hC, N, N, ldc);

    // copy matrix is easy in STL; hB = hA: save a copy in hB which will be output of CPU BLAS
    // hB = hA;

    // copy data from CPU to device
    hipMemcpy(dA, hA.data(), sizeof(T) * A_size, hipMemcpyHostToDevice);
    hipMemcpy(dC, hC.data(), sizeof(T) * C_size, hipMemcpyHostToDevice);

    /* =====================================================================
           ROCBLAS
    =================================================================== */
    if(argus.timing)
    {
        gpu_time_used = get_time_us(); // in microseconds
    }

    for(int iter = 0; iter < 1; iter++)
    {
        status = hipblasSyrkFn(handle, uplo, transA, N, K, (T*)&alpha, dA, lda, (T*)&beta, dC, ldc);

        if(status != HIPBLAS_STATUS_SUCCESS)
        {
            hipblasDestroy(handle);
            return status;
        }
    }

    // copy output from device to CPU
    hipMemcpy(hC2.data(), dC, sizeof(T) * C_size, hipMemcpyDeviceToHost);

    if(argus.unit_check)
    {
        /* =====================================================================
           CPU BLAS
        =================================================================== */
        cblas_syrk<T>(uplo, transA, N, K, alpha, hA.data(), lda, beta, hC, ldc);

        // enable unit check, notice unit check is not invasive, but norm check is,
        // unit check and norm check can not be interchanged their order
        if(argus.unit_check)
        {
            unit_check_general<T>(N, N, ldc, hC2.data(), hC.data());
        }
    }

    hipblasDestroy(handle);
    return HIPBLAS_STATUS_SUCCESS;
}

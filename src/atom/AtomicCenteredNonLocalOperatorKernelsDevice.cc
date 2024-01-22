// ---------------------------------------------------------------------
//
// Copyright (c) 2017-2022 The Regents of the University of Michigan and DFT-FE
// authors.
//
// This file is part of the DFT-FE code.
//
// The DFT-FE code is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE at
// the top level of the DFT-FE distribution.
//
// ---------------------------------------------------------------------
//
// @author Kartick Ramakrishnan
//

#include <AtomicCenteredNonLocalOperatorKernelsDevice.h>
#include <deviceKernelsGeneric.h>
#include <DeviceAPICalls.h>
#include <DeviceDataTypeOverloads.h>
#include <DeviceTypeConfig.h>
#include <DeviceKernelLauncherConstants.h>
namespace dftfe
{
  namespace
  {
    template <typename ValueType>
    __global__ void
    copyFromParallelNonLocalVecToAllCellsVecKernel(
      const unsigned int numWfcs,
      const unsigned int numNonLocalCells,
      const unsigned int maxSingleAtomPseudoWfc,
      const ValueType *  sphericalFnTimesWfcParallelVec,
      ValueType *        sphericalFnTimesWfcAllCellsVec,
      const int *        indexMapPaddedToParallelVec)
    {
      const unsigned int globalThreadId = blockIdx.x * blockDim.x + threadIdx.x;
      const unsigned int numberEntries =
        numNonLocalCells * maxSingleAtomPseudoWfc * numWfcs;

      for (unsigned int index = globalThreadId; index < numberEntries;
           index += blockDim.x * gridDim.x)
        {
          const unsigned int blockIndex      = index / numWfcs;
          const unsigned int intraBlockIndex = index % numWfcs;
          const int mappedIndex = indexMapPaddedToParallelVec[blockIndex];
          if (mappedIndex != -1)
            sphericalFnTimesWfcAllCellsVec[index] =
              sphericalFnTimesWfcParallelVec[mappedIndex * numWfcs +
                                             intraBlockIndex];
        }
    }

    template <typename ValueType>
    __global__ void
    copyToDealiiParallelNonLocalVecKernel(
      const unsigned int  numWfcs,
      const unsigned int  totalPseudoWfcs,
      const ValueType *   sphericalFnTimesWfcParallelVec,
      ValueType *         sphericalFnTimesWfcDealiiParallelVec,
      const unsigned int *indexMapDealiiParallelNumbering)
    {
      const unsigned int globalThreadId = blockIdx.x * blockDim.x + threadIdx.x;
      const unsigned int numberEntries  = totalPseudoWfcs * numWfcs;

      for (unsigned int index = globalThreadId; index < numberEntries;
           index += blockDim.x * gridDim.x)
        {
          const unsigned int blockIndex      = index / numWfcs;
          const unsigned int intraBlockIndex = index % numWfcs;
          const unsigned int mappedIndex =
            indexMapDealiiParallelNumbering[blockIndex];

          sphericalFnTimesWfcDealiiParallelVec[mappedIndex * numWfcs +
                                               intraBlockIndex] =
            sphericalFnTimesWfcParallelVec[index];
        }
    }



  } // namespace

  namespace AtomicCenteredNonLocalOperatorKernelsDevice
  {
    template <typename ValueType>
    void
    copyFromParallelNonLocalVecToAllCellsVec(
      const unsigned int numWfcs,
      const unsigned int numNonLocalCells,
      const unsigned int maxSingleAtomContribution,
      const ValueType *  sphericalFnTimesWfcParallelVec,
      ValueType *        sphericalFnTimesWfcAllCellsVec,
      const int *        indexMapPaddedToParallelVec)
    {
#ifdef DFTFE_WITH_DEVICE_LANG_CUDA
      copyFromParallelNonLocalVecToAllCellsVecKernel<<<
        (numWfcs + (dftfe::utils::DEVICE_BLOCK_SIZE - 1)) /
          dftfe::utils::DEVICE_BLOCK_SIZE * numNonLocalCells *
          maxSingleAtomContribution,
        dftfe::utils::DEVICE_BLOCK_SIZE>>>(
        numWfcs,
        numNonLocalCells,
        maxSingleAtomContribution,
        dftfe::utils::makeDataTypeDeviceCompatible(
          sphericalFnTimesWfcParallelVec),
        dftfe::utils::makeDataTypeDeviceCompatible(
          sphericalFnTimesWfcAllCellsVec),
        indexMapPaddedToParallelVec);
#elif DFTFE_WITH_DEVICE_LANG_HIP
      hipLaunchKernelGGL(copyFromParallelNonLocalVecToAllCellsVecKernel,
                         (numWfcs + (dftfe::utils::DEVICE_BLOCK_SIZE - 1)) /
                           dftfe::utils::DEVICE_BLOCK_SIZE * numNonLocalCells *
                           maxSingleAtomContribution,
                         dftfe::utils::DEVICE_BLOCK_SIZE,
                         0,
                         0,
                         numWfcs,
                         numNonLocalCells,
                         maxSingleAtomContribution,
                         dftfe::utils::makeDataTypeDeviceCompatible(
                           sphericalFnTimesWfcParallelVec),
                         dftfe::utils::makeDataTypeDeviceCompatible(
                           sphericalFnTimesWfcAllCellsVec),
                         indexMapPaddedToParallelVec);
#endif
    }
    template <typename ValueType>
    void
    copyToDealiiParallelNonLocalVec(
      const unsigned int  numWfcs,
      const unsigned int  totalEntries,
      const ValueType *   sphericalFnTimesWfcParallelVec,
      ValueType *         sphericalFnTimesWfcDealiiParallelVec,
      const unsigned int *indexMapDealiiParallelNumbering)
    {
#ifdef DFTFE_WITH_DEVICE_LANG_CUDA
      copyToDealiiParallelNonLocalVecKernel<<<
        (numWfcs + (dftfe::utils::DEVICE_BLOCK_SIZE - 1)) /
          dftfe::utils::DEVICE_BLOCK_SIZE * totalEntries,
        dftfe::utils::DEVICE_BLOCK_SIZE>>>(
        numWfcs,
        totalEntries,
        dftfe::utils::makeDataTypeDeviceCompatible(
          sphericalFnTimesWfcParallelVec),
        dftfe::utils::makeDataTypeDeviceCompatible(
          sphericalFnTimesWfcDealiiParallelVec),
        indexMapDealiiParallelNumbering);
#elif DFTFE_WITH_DEVICE_LANG_HIP
      hipLaunchKernelGGL(copyToDealiiParallelNonLocalVecKernel,
                         (numWfcs + (dftfe::utils::DEVICE_BLOCK_SIZE - 1)) /
                           dftfe::utils::DEVICE_BLOCK_SIZE * totalEntries,
                         dftfe::utils::DEVICE_BLOCK_SIZE,
                         0,
                         0,
                         numWfcs,
                         totalEntries,
                         dftfe::utils::makeDataTypeDeviceCompatible(
                           sphericalFnTimesWfcParallelVec),
                         dftfe::utils::makeDataTypeDeviceCompatible(
                           sphericalFnTimesWfcDealiiParallelVec),
                         indexMapDealiiParallelNumbering);
#endif
    }

    template void
    copyToDealiiParallelNonLocalVec(
      const unsigned int  numWfcs,
      const unsigned int  totalEntries,
      const double *      sphericalFnTimesWfcParallelVec,
      double *            sphericalFnTimesWfcDealiiParallelVec,
      const unsigned int *indexMapDealiiParallelNumbering);

    template void
    copyToDealiiParallelNonLocalVec(
      const unsigned int          numWfcs,
      const unsigned int          totalEntries,
      const std::complex<double> *sphericalFnTimesWfcParallelVec,
      std::complex<double> *      sphericalFnTimesWfcDealiiParallelVec,
      const unsigned int *        indexMapDealiiParallelNumbering);

    template void
    copyFromParallelNonLocalVecToAllCellsVec(
      const unsigned int numWfcs,
      const unsigned int numNonLocalCells,
      const unsigned int maxSingleAtomContribution,
      const double *     sphericalFnTimesWfcParallelVec,
      double *           sphericalFnTimesWfcAllCellsVec,
      const int *        indexMapPaddedToParallelVec);


    template void
    copyFromParallelNonLocalVecToAllCellsVec(
      const unsigned int          numWfcs,
      const unsigned int          numNonLocalCells,
      const unsigned int          maxSingleAtomContribution,
      const std::complex<double> *sphericalFnTimesWfcParallelVec,
      std::complex<double> *      sphericalFnTimesWfcAllCellsVec,
      const int *                 indexMapPaddedToParallelVec);

  } // namespace AtomicCenteredNonLocalOperatorKernelsDevice

} // namespace dftfe

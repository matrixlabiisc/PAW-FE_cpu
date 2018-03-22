// ---------------------------------------------------------------------
//
// Copyright (c) 2017 The Regents of the University of Michigan and DFT-FE authors.
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
// @author Phani Motamarri (2017)
//
template<unsigned int FEOrder>
void eigenClass<FEOrder>::computeNonLocalHamiltonianTimesX(const std::vector<vectorType> &src,
							   std::vector<vectorType>       &dst)
{

  //
  //get FE data
  //
  QGauss<3>  quadrature_formula(C_num1DQuad<FEOrder>());
  FEValues<3> fe_values (dftPtr->FEEigen, quadrature_formula, update_values);

  //
  //get access to triangulation objects from meshGenerator class
  //
  const int kPointIndex = dftPtr->d_kPointIndex;
  const unsigned int dofs_per_cell = dftPtr->FEEigen.dofs_per_cell;

#ifdef ENABLE_PERIODIC_BC
  int numberNodesPerElement = dftPtr->FEEigen.dofs_per_cell/2;
#else
  int numberNodesPerElement = dftPtr->FEEigen.dofs_per_cell;
#endif

  //
  //compute nonlocal projector ket times x i.e C^{T}*X 
  //
  std::vector<dealii::types::global_dof_index> local_dof_indices(dofs_per_cell);
#ifdef ENABLE_PERIODIC_BC
  std::vector<std::vector<std::complex<double> > > projectorKetTimesVector;
#else
  std::vector<std::vector<double> > projectorKetTimesVector;
#endif

  //
  //get number of Nonlocal atoms
  //
  const int numberNonLocalAtoms = dftPtr->d_nonLocalAtomGlobalChargeIds.size();
  int numberWaveFunctions = src.size();
  projectorKetTimesVector.clear();

  //
  //allocate memory for matrix-vector product
  //
  projectorKetTimesVector.resize(numberNonLocalAtoms);
  for(unsigned int iAtom = 0; iAtom < numberNonLocalAtoms; ++iAtom)
    {
      const int numberSingleAtomPseudoWaveFunctions = dftPtr->d_numberPseudoAtomicWaveFunctions[iAtom];
      projectorKetTimesVector[iAtom].resize(numberWaveFunctions*numberSingleAtomPseudoWaveFunctions,0.0);
    }
  
  //
  //some useful vectors
  //
#ifdef ENABLE_PERIODIC_BC
  std::vector<std::complex<double> > inputVectors(numberNodesPerElement*numberWaveFunctions,0.0);
#else
  std::vector<double> inputVectors(numberNodesPerElement*numberWaveFunctions,0.0);
#endif
  

  //
  //parallel loop over all elements to compute nonlocal projector ket times x i.e C^{T}*X 
  //
  typename DoFHandler<3>::active_cell_iterator cell = dftPtr->dofHandlerEigen.begin_active(), endc = dftPtr->dofHandlerEigen.end();
  int iElem = -1;
  for(; cell!=endc; ++cell) 
    {
      if(cell->is_locally_owned())
	{
	  iElem ++;
	  cell->get_dof_indices(local_dof_indices);

	  unsigned int index=0;

	  std::vector<double> temp(dofs_per_cell,0.0);
	  for (std::vector<vectorType>::const_iterator it=src.begin(); it!=src.end(); it++)
	    {
#ifdef ENABLE_PERIODIC_BC	      
	      (*it).extract_subvector_to(local_dof_indices.begin(), local_dof_indices.end(), temp.begin());
	      for(unsigned int idof = 0; idof < dofs_per_cell; ++idof)
		{
		  //
		  //This is the component index 0(real) or 1(imag).
		  //
		  const unsigned int ck = fe_values.get_fe().system_to_component_index(idof).first; 
		  const unsigned int iNode = fe_values.get_fe().system_to_component_index(idof).second;
		  if(ck == 0)
		    inputVectors[numberNodesPerElement*index + iNode].real(temp[idof]);
		  else
		    inputVectors[numberNodesPerElement*index + iNode].imag(temp[idof]);
		}
#else
	      (*it).extract_subvector_to(local_dof_indices.begin(), local_dof_indices.end(), inputVectors.begin()+numberNodesPerElement*index);
#endif
	      index++;
	    }


	  for(unsigned int iAtom = 0; iAtom < dftPtr->d_nonLocalAtomIdsInElement[iElem].size();++iAtom)
	    {
	      int atomId = dftPtr->d_nonLocalAtomIdsInElement[iElem][iAtom];
	      int numberPseudoWaveFunctions = dftPtr->d_numberPseudoAtomicWaveFunctions[atomId];
	      int nonZeroElementMatrixId = dftPtr->d_sparsityPattern[atomId][iElem];
#ifdef ENABLE_PERIODIC_BC
	      char transA = 'C';
	      char transB = 'N';
	      std::complex<double> alpha = 1.0;
	      std::complex<double> beta = 1.0;
	      zgemm_(&transA,
		     &transB,
		     &numberPseudoWaveFunctions,
		     &numberWaveFunctions,
		     &numberNodesPerElement,
		     &alpha,
		     &dftPtr->d_nonLocalProjectorElementMatrices[atomId][nonZeroElementMatrixId][kPointIndex][0],
		     &numberNodesPerElement,
		     &inputVectors[0],
		     &numberNodesPerElement,
		     &beta,
		     &projectorKetTimesVector[atomId][0],
		     &numberPseudoWaveFunctions);
#else
	      char transA = 'T';
	      char transB = 'N';
	      double alpha = 1.0;
	      double beta = 1.0;
	      dgemm_(&transA,
		     &transB,
		     &numberPseudoWaveFunctions,
		     &numberWaveFunctions,
		     &numberNodesPerElement,
		     &alpha,
		     &dftPtr->d_nonLocalProjectorElementMatrices[atomId][nonZeroElementMatrixId][kPointIndex][0],
		     &numberNodesPerElement,
		     &inputVectors[0],
		     &numberNodesPerElement,
		     &beta,
		     &projectorKetTimesVector[atomId][0],
		     &numberPseudoWaveFunctions);
#endif
	    }

	}

    }//element loop

#ifdef ENABLE_PERIODIC_BC
  std::vector<std::complex<double> > tempVectorloc;
  std::vector<std::complex<double> > tempVector;
#else
  std::vector<double> tempVector;
#endif

  for(unsigned int iAtom = 0; iAtom < numberNonLocalAtoms; ++iAtom)
    {
      const unsigned int numberPseudoWaveFunctions = dftPtr->d_numberPseudoAtomicWaveFunctions[iAtom];
      for(unsigned int iWave = 0; iWave < numberWaveFunctions; ++iWave)
	{
	  for(unsigned int iPseudoAtomicWave = 0; iPseudoAtomicWave < numberPseudoWaveFunctions; ++iPseudoAtomicWave)
	    {
#ifdef ENABLE_PERIODIC_BC
	      tempVectorloc.push_back(projectorKetTimesVector[iAtom][numberPseudoWaveFunctions*iWave + iPseudoAtomicWave]);
#else
	      tempVector.push_back(projectorKetTimesVector[iAtom][numberPseudoWaveFunctions*iWave + iPseudoAtomicWave]);
#endif
	    }

	}

    }



#ifdef ENABLE_PERIODIC_BC
  int size = tempVectorloc.size();
  tempVector.resize(size);
  MPI_Allreduce(&tempVectorloc[0],
		&tempVector[0],
		size,
		MPI_C_DOUBLE_COMPLEX,
		MPI_SUM,
		mpi_communicator);
#else
  Utilities::MPI::sum(tempVector,
  		      mpi_communicator,
  		      tempVector);
#endif

  int count = 0;
  for(unsigned int iAtom = 0; iAtom < numberNonLocalAtoms; ++iAtom)
    {
      const unsigned int numberPseudoWaveFunctions = dftPtr->d_numberPseudoAtomicWaveFunctions[iAtom];
      for(unsigned int iWave = 0; iWave < numberWaveFunctions; ++iWave)
	{
	  for(unsigned int iPseudoAtomicWave = 0; iPseudoAtomicWave < numberPseudoWaveFunctions; ++iPseudoAtomicWave)
	    {
	      projectorKetTimesVector[iAtom][numberPseudoWaveFunctions*iWave + iPseudoAtomicWave] = tempVector[count];
	      count++;
	    }

	}
    }

  //
  //compute V*C^{T}*X
  //
  for(unsigned int iAtom = 0; iAtom < numberNonLocalAtoms; ++iAtom)
    {
      const unsigned int numberPseudoWaveFunctions =  dftPtr->d_numberPseudoAtomicWaveFunctions[iAtom];
      for(unsigned int iWave = 0; iWave < numberWaveFunctions; ++iWave)
	{
	  for(unsigned int iPseudoAtomicWave = 0; iPseudoAtomicWave < numberPseudoWaveFunctions; ++iPseudoAtomicWave)
	    projectorKetTimesVector[iAtom][numberPseudoWaveFunctions*iWave + iPseudoAtomicWave] *= dftPtr->d_nonLocalPseudoPotentialConstants[iAtom][iPseudoAtomicWave];
	}
    }
  

  char transA1 = 'N';
  char transB1 = 'N';
 	  
  //
  //access elementIdsInAtomCompactSupport
  //
  
#ifdef ENABLE_PERIODIC_BC
  std::vector<std::complex<double> > outputVectors(numberNodesPerElement*numberWaveFunctions,0.0);
#else
  std::vector<double> outputVectors(numberNodesPerElement*numberWaveFunctions,0.0);
#endif

  //
  //compute C*V*C^{T}*x
  //
  for(unsigned int iAtom = 0; iAtom < numberNonLocalAtoms; ++iAtom)
    {
      int numberPseudoWaveFunctions =  dftPtr->d_numberPseudoAtomicWaveFunctions[iAtom];
      for(unsigned int iElemComp = 0; iElemComp < dftPtr->d_elementIteratorsInAtomCompactSupport[iAtom].size(); ++iElemComp)
	{

	  DoFHandler<3>::active_cell_iterator cell = dftPtr->d_elementIteratorsInAtomCompactSupport[iAtom][iElemComp];

#ifdef ENABLE_PERIODIC_BC
	  std::complex<double> alpha1 = 1.0;
	  std::complex<double> beta1 = 0.0;

	  zgemm_(&transA1,
		 &transB1,
		 &numberNodesPerElement,
		 &numberWaveFunctions,
		 &numberPseudoWaveFunctions,
		 &alpha1,
		 &dftPtr->d_nonLocalProjectorElementMatrices[iAtom][iElemComp][kPointIndex][0],
		 &numberNodesPerElement,
		 &projectorKetTimesVector[iAtom][0],
		 &numberPseudoWaveFunctions,
		 &beta1,
		 &outputVectors[0],
		 &numberNodesPerElement);
#else
	  double alpha1 = 1.0;
	  double beta1 = 0.0;

	  dgemm_(&transA1,
		 &transB1,
		 &numberNodesPerElement,
		 &numberWaveFunctions,
		 &numberPseudoWaveFunctions,
		 &alpha1,
		 &dftPtr->d_nonLocalProjectorElementMatrices[iAtom][iElemComp][kPointIndex][0],
		 &numberNodesPerElement,
		 &projectorKetTimesVector[iAtom][0],
		 &numberPseudoWaveFunctions,
		 &beta1,
		 &outputVectors[0],
		 &numberNodesPerElement);
#endif

	  cell->get_dof_indices(local_dof_indices);

#ifdef ENABLE_PERIODIC_BC
	  unsigned int index = 0;
	  std::vector<double> temp(dofs_per_cell,0.0);
	  for(std::vector<vectorType>::iterator it = dst.begin(); it != dst.end(); ++it)
	    {
	      for(unsigned int idof = 0; idof < dofs_per_cell; ++idof)
		{
		  const unsigned int ck = fe_values.get_fe().system_to_component_index(idof).first;
		  const unsigned int iNode = fe_values.get_fe().system_to_component_index(idof).second;
		  
		  if(ck == 0)
		    temp[idof] = outputVectors[numberNodesPerElement*index + iNode].real();
		  else
		    temp[idof] = outputVectors[numberNodesPerElement*index + iNode].imag();

		}
	      dftPtr->constraintsNoneEigen.distribute_local_to_global(temp.begin(), temp.end(),local_dof_indices.begin(), *it);
	      index++;
	    }
#else
	  std::vector<double>::iterator iter = outputVectors.begin();
	  for (std::vector<vectorType>::iterator it=dst.begin(); it!=dst.end(); ++it)
	    {
	      dftPtr->constraintsNoneEigen.distribute_local_to_global(iter, iter+numberNodesPerElement,local_dof_indices.begin(), *it);
	      iter+=numberNodesPerElement;
	    }
#endif

	}

    }

}

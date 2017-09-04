//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source License.
// See LICENSE file in top directory for details.
//
// Copyright (c) 2016 Jeongnim Kim and QMCPACK developers.
//
// File developed by: Jeongnim Kim, jeongnim.kim@gmail.com, University of Illinois at Urbana-Champaign
//                    Jeremy McMinnis, jmcminis@gmail.com, University of Illinois at Urbana-Champaign
//                    Jaron T. Krogel, krogeljt@ornl.gov, Oak Ridge National Laboratory
//                    Raymond Clay III, j.k.rofling@gmail.com, Lawrence Livermore National Laboratory
//                    Mark A. Berrill, berrillma@ornl.gov, Oak Ridge National Laboratory
//
// File created by: Jeongnim Kim, jeongnim.kim@gmail.com, University of Illinois at Urbana-Champaign
//////////////////////////////////////////////////////////////////////////////////////
    
    


#include "QMCDrivers/CorrelatedSampling/CSVMC.h"
#include "QMCDrivers/CorrelatedSampling/CSVMCUpdateAll.h"
#include "QMCDrivers/CorrelatedSampling/CSVMCUpdatePbyP.h"
#include "Estimators/CSEnergyEstimator.h"
#include "QMCDrivers/DriftOperators.h"
#include "OhmmsApp/RandomNumberControl.h"
#include "Message/OpenMP.h"
#include "Message/CommOperators.h"
#include "tau/profiler.h"
#include <qmc_common.h>
//#define ENABLE_VMC_OMP_MASTER
#include "ADIOS/ADIOS_profile.h"
#if !defined(REMOVE_TRACEMANAGER)
#include "Estimators/TraceManager.h"
#else
typedef int TraceManager;
#endif

namespace qmcplusplus
{

/// Constructor.
CSVMC::CSVMC(MCWalkerConfiguration& w, TrialWaveFunction& psi, QMCHamiltonian& h, 
                           HamiltonianPool& hpool, WaveFunctionPool& ppool):
  QMCDriver(w,psi,h,ppool), CloneManager(hpool), multiEstimator(0), Mover(0), UseDrift("yes")
{
  RootName = "csvmc";
  QMCType ="CSVMC";
  m_param.add(UseDrift,"useDrift","string");
  m_param.add(UseDrift,"usedrift","string");
  m_param.add(UseDrift,"use_drift","string");
  equilBlocks=-1;
  m_param.add(equilBlocks,"equilBlocks","int");
  QMCDriverMode.set(QMC_MULTIPLE,1);
  //Add the primary h and psi, extra H and Psi pairs will be added by QMCMain
  //for(int ipsi=0; ipsi<ppool.size(); ipsi++)
 // {
	//  add_H_and_Psi(&h,&psi);
 // }
}

/** allocate internal data here before run() is called
 * @author SIMONE
 *
 * See QMCDriver::process
 */
bool CSVMC::put(xmlNodePtr q)
{
 /* int nPsi=H1.size();
  //for(int ipsi=0; ipsi<nPsi; ipsi++)
  //  H1[ipsi]->add2WalkerProperty(W);
  Estimators = branchEngine->getEstimatorManager();
  if(Estimators == 0)
  {
  //  Estimators = new EstimatorManager(myComm);
    multiEstimator = new CSEnergyEstimator(H,nPsi);
    Estimators->add(multiEstimator,Estimators->MainEstimatorName);
    branchEngine->setEstimatorManager(Estimators);
  }
  H1[0]->setPrimary(true);
  for(int ipsi=1; ipsi<nPsi; ipsi++)
    H1[ipsi]->setPrimary(false);
  return true;*/
  
    //grep minimumTargetWalker
  int target_min=-1;
  ParameterSet p;
  p.add(target_min,"minimumtargetwalkers","int"); //p.add(target_min,"minimumTargetWalkers","int"); 
  p.add(target_min,"minimumsamples","int"); //p.add(target_min,"minimumSamples","int");
  p.put(q);

  app_log() << "\n<vmc function=\"put\">"
    << "\n  qmc_counter=" << qmc_common.qmc_counter << "  my_counter=" << MyCounter<< std::endl;
  if(qmc_common.qmc_counter && MyCounter)
  {
    nSteps=prevSteps;
    nStepsBetweenSamples=prevStepsBetweenSamples;
  }
  else
  {
    int nw=W.getActiveWalkers();
    //compute samples and overwrite steps for the given samples
    int Nthreads = omp_get_max_threads();
    int Nprocs=myComm->size();
    //target samples set by samples or samplesperthread/dmcwalkersperthread
    nTargetPopulation=std::max(nTargetPopulation,nSamplesPerThread*Nprocs*Nthreads);
    nTargetSamples=static_cast<int>(std::ceil(nTargetPopulation));

    if(nTargetSamples)
    {
      int nwtot=nw*Nprocs;  //total number of walkers used by this qmcsection
      nTargetSamples=std::max(nwtot,nTargetSamples);
      if(target_min>0) 
      { 
        nTargetSamples=std::max(nTargetSamples,target_min);
        nTargetPopulation=std::max(nTargetPopulation,static_cast<RealType>(target_min));
      }
      nTargetSamples=((nTargetSamples+nwtot-1)/nwtot)*nwtot; // nTargetSamples are always multiples of total number of walkers
      nSamplesPerThread=nTargetSamples/Nprocs/Nthreads;
      int ns_target=nTargetSamples*nStepsBetweenSamples; //total samples to generate
      int ns_per_step=Nprocs*nw;  //total samples per step
      nSteps=std::max(nSteps,(ns_target/ns_per_step+nBlocks-1)/nBlocks);
      Period4WalkerDump=nStepsBetweenSamples=(ns_per_step*nSteps*nBlocks)/nTargetSamples;
    }
    else
    {
      Period4WalkerDump = nStepsBetweenSamples=(nBlocks+1)*nSteps; //some positive number, not used
      nSamplesPerThread=0;
    }
  }
  prevSteps=nSteps;
  prevStepsBetweenSamples=nStepsBetweenSamples;

  app_log() << "  time step      = " << Tau << std::endl;
  app_log() << "  blocks         = " << nBlocks << std::endl;
  app_log() << "  steps          = " << nSteps << std::endl;
  app_log() << "  substeps       = " << nSubSteps << std::endl;
  app_log() << "  current        = " << CurrentStep << std::endl;
  app_log() << "  target samples = " << nTargetPopulation << std::endl;
  app_log() << "  walkers/mpi    = " << W.getActiveWalkers() << std::endl << std::endl;
  app_log() << "  stepsbetweensamples = " << nStepsBetweenSamples << std::endl;

  m_param.get(app_log());

  if(DumpConfig)
  {
    app_log() << "  DumpConfig==true Configurations are dumped to config.h5 with a period of " << Period4CheckPoint << " blocks" << std::endl;
  }
  else
  {
    app_log() << "  DumpConfig==false Nothing (configurations, state) will be saved." << std::endl;
  }

  if (Period4WalkerDump>0)
    app_log() << "  Walker Samples are dumped every " << Period4WalkerDump << " steps." << std::endl;

  app_log() << "</vmc>" << std::endl;
  app_log().flush();

  return true;
}

/** Run the CSVMC algorithm.
 *
 * Similar to VMC::run
 */
bool CSVMC::run()
{
 /* resetRun();
  Mover->startRun(nBlocks,true);
  IndexType block = 0;
  int nPsi=Psi1.size();
  do
  {
    IndexType step = 0;
    nAccept = 0;
    nReject=0;
    Mover->startBlock(nSteps);
    do
    {
      Mover->advanceWalkers(W.begin(), W.end(),false);
      step++;
      CurrentStep++;
      Movers[0]->accumulate(W);
    }
    while(step<nSteps);
   // multiEstimator->evaluateDiff();
    //Modify Norm.
    if(block < equilBlocks)
      Mover->updateNorms();
    Mover->stopBlock();
    ++block;
    //record the current configuration
    recordBlock(block);
    //if(QMCDriverMode[QMC_UPDATE_MODE] && CurrentStep%100 == 0)
    //  Mover->updateCSWalkers(W.begin(),W.end());
  }
  while(block<nBlocks);
  Mover->stopRun();
  //finalize a qmc section
  return finalize(block); */
  
  resetRun();
  //start the main estimator
  Estimators->start(nBlocks);
  for (int ip=0; ip<NumThreads; ++ip)
    CSMovers[ip]->startRun(nBlocks,false);

#if !defined(REMOVE_TRACEMANAGER)
  Traces->startRun(nBlocks,traceClones);
#endif
  const bool has_collectables=W.Collectables.size();
  ADIOS_PROFILE::profile_adios_init(nBlocks);
  for (int block=0; block<nBlocks; ++block)
  {
    ADIOS_PROFILE::profile_adios_start_comp(block);
    #pragma omp parallel
    {
      int ip=omp_get_thread_num();
      //IndexType updatePeriod=(QMCDriverMode[QMC_UPDATE_MODE])?Period4CheckProperties:(nBlocks+1)*nSteps;
      IndexType updatePeriod=(QMCDriverMode[QMC_UPDATE_MODE])?Period4CheckProperties:0;
      //assign the iterators and resuse them
      MCWalkerConfiguration::iterator wit(W.begin()+wPerNode[ip]), wit_end(W.begin()+wPerNode[ip+1]);
      CSMovers[ip]->startBlock(nSteps);
      int now_loc=CurrentStep;
      RealType cnorm=1.0/static_cast<RealType>(wPerNode[ip+1]-wPerNode[ip]);
      for (int step=0; step<nSteps; ++step)
      {
        CSMovers[ip]->set_step(now_loc);
        //collectables are reset, it is accumulated while advancing walkers
        wClones[ip]->resetCollectables();
        CSMovers[ip]->advanceWalkers(wit,wit_end,false);
        if(has_collectables)
          wClones[ip]->Collectables *= cnorm;
        CSMovers[ip]->accumulate(wit,wit_end);
        ++now_loc;
        //app_log()<<"==step==\n";
        //if (updatePeriod&& now_loc%updatePeriod==0) Movers[ip]->updateWalkers(wit,wit_end);
        if (Period4WalkerDump&& now_loc%Period4WalkerDump==0)
          wClones[ip]->saveEnsemble(wit,wit_end);
//           if(storeConfigs && (now_loc%storeConfigs == 0))
//             ForwardWalkingHistory.storeConfigsForForwardWalking(*wClones[ip]);
      }
      CSMovers[ip]->stopBlock(false);
     // app_log()<<"THREAD "<<ip<< std::endl;
     // CSMovers[ip]->updateAvgWeights();
      
    }//end-of-parallel for
    //Estimators->accumulateCollectables(wClones,nSteps);
    CurrentStep+=nSteps;
    Estimators->stopBlock(estimatorClones);
    ADIOS_PROFILE::profile_adios_end_comp(block);
    ADIOS_PROFILE::profile_adios_start_trace(block);
#if !defined(REMOVE_TRACEMANAGER)
    Traces->write_buffers(traceClones, block);
#endif
    ADIOS_PROFILE::profile_adios_end_trace(block);
    ADIOS_PROFILE::profile_adios_start_checkpoint(block);
    if(storeConfigs)
      recordBlock(block);
    ADIOS_PROFILE::profile_adios_end_checkpoint(block);
  }//block
  ADIOS_PROFILE::profile_adios_finalize(myComm, nBlocks);
  Estimators->stop(estimatorClones);
  for (int ip=0; ip<NumThreads; ++ip)
    CSMovers[ip]->stopRun2();
#if !defined(REMOVE_TRACEMANAGER)
  Traces->stopRun();
#endif
  //copy back the random states
  for (int ip=0; ip<NumThreads; ++ip)
    *(RandomNumberControl::Children[ip])=*(Rng[ip]);
  ///write samples to a file
  bool wrotesamples=DumpConfig;
  if(DumpConfig)
  {
    wrotesamples=W.dumpEnsemble(wClones,wOut,myComm->size(),nBlocks);
    if(wrotesamples)
      app_log() << "  samples are written to the config.h5" << std::endl;
  }
  //finalize a qmc section
  return finalize(nBlocks,!wrotesamples);
}

void CSVMC::resetRun()
{
  H1[0]->setPrimary(true);
  IndexType nPsi=Psi1.size();
  for(int ipsi=1; ipsi<nPsi; ipsi++)
    H1[ipsi]->setPrimary(false);
    

  makeClones(W,Psi1,H1);
  FairDivideLow(W.getActiveWalkers(),NumThreads,wPerNode);
 
  if(NumThreads>1) APP_ABORT("OpenMP Parallelization for CSVMC not working at the moment");

  app_log() << "  Initial partition of walkers ";
  copy(wPerNode.begin(),wPerNode.end(),std::ostream_iterator<int>(app_log()," "));
  app_log() << std::endl;
  
 
  if(Movers.empty())
  {
	CSMovers.resize(NumThreads,0);
    branchClones.resize(NumThreads,0);
    estimatorClones.resize(NumThreads,0);
    traceClones.resize(NumThreads,0);
    Rng.resize(NumThreads,0);
 
    
    #pragma omp parallel for
    for(int ip=0; ip<NumThreads; ++ip)
    {
	  std::ostringstream os;
	  estimatorClones[ip]= new EstimatorManager(*Estimators);//,*hClones[ip]);
      estimatorClones[ip]->resetTargetParticleSet(*wClones[ip]);
      estimatorClones[ip]->setCollectionMode(false);
#if !defined(REMOVE_TRACEMANAGER)
	  traceClones[ip] = Traces->makeClone();
#endif
	  Rng[ip]=new RandomGenerator_t(*(RandomNumberControl::Children[ip]));
	  
	  //HPoolClones[0]->setPrimary(true);
	//  for(int ipsi=1; ipsi<nPsi; ipsi++)
   //     HPoolClones[ipsi]->setPrimary(false);
     
      branchClones[ip] = new BranchEngineType(*branchEngine);
	 //  hClones[ip]->setRandomGenerator(Rng[ip]);
      if(QMCDriverMode[QMC_UPDATE_MODE])
      {
        if (UseDrift == "yes")
        {
          os <<"  Using particle-by-particle update with drift "<< std::endl;
          APP_ABORT("Particle-by-particle moves with drift not supported yet\n");
        }
        else
        {
          os << "  Using particle-by-particle update with no drift" << std::endl;
          CSMovers[ip]=new CSVMCUpdatePbyPWithDriftFast(*wClones[ip],PsiPoolClones[ip],HPoolClones[ip],*Rng[ip]);
        }
      }
      else
      {
      
        if (UseDrift == "yes")
        {
          os << "  Using walker-by-walker update with Drift " << std::endl;
          CSMovers[ip]=new CSVMCUpdateAllWithDrift(*wClones[ip],PsiPoolClones[ip],HPoolClones[ip],*Rng[ip]);
        }
        else
        {
          os << "  Using walker-by-walker update " << std::endl;
          CSMovers[ip]=new CSVMCUpdateAll(*wClones[ip],PsiPoolClones[ip],HPoolClones[ip],*Rng[ip]);
        }
      }
      if(ip==0)
        app_log() << os.str() << std::endl;

      //traceClones[ip]->transfer_state_from(*Traces);

      CSMovers[ip]->put(qmcNode);
      //CSMovers[ip]->Psi1=PsiPoolClones[ip];
     // CSMovers[ip]->H1=HPoolClones[ip];
   //   CSMovers[ip]->multiEstimator= Estimators->getEstimator(Estimators->MainEstimatorName);
   
      //estimatorClones[ip]->add(CSMovers[ip]->multiEstimator,Estimators->MainEstimatorName);;
      CSMovers[ip]->resetRun( branchClones[ip], estimatorClones[ip],traceClones[ip]);
    }

  }
#if !defined(REMOVE_TRACEMANAGER)
  else
  {
    #pragma omp parallel for
    for(int ip=0; ip<NumThreads; ++ip)
    {
      traceClones[ip]->transfer_state_from(*Traces);
    }
  }
#endif
  
  app_log() << "  Total Sample Size   =" << nTargetSamples << std::endl;
  app_log() << "  Walker distribution on root = ";
  copy(wPerNode.begin(),wPerNode.end(),std::ostream_iterator<int>(app_log()," "));
  app_log() << std::endl;
  //app_log() << "  Sample Size per node=" << samples_this_node << std::endl;
  //for (int ip=0; ip<NumThreads; ++ip)
  //  app_log()  << "    Sample size for thread " <<ip<<" = " << samples_th[ip] << std::endl;
  app_log().flush();
  
  #pragma omp parallel for
  for(int ip=0; ip<NumThreads; ++ip)
  {
    //int ip=omp_get_thread_num();
    CSMovers[ip]->put(qmcNode);
    CSMovers[ip]->resetRun(branchClones[ip],estimatorClones[ip],traceClones[ip]);
    if (QMCDriverMode[QMC_UPDATE_MODE])
     CSMovers[ip]->initCSWalkersForPbyP(W.begin()+wPerNode[ip],W.begin()+wPerNode[ip+1], nWarmupSteps>0);
    else
      CSMovers[ip]->initCSWalkers(W.begin()+wPerNode[ip],W.begin()+wPerNode[ip+1], nWarmupSteps>0);
//       if (UseDrift != "rn")
//       {
    for (int prestep=0; prestep<nWarmupSteps; ++prestep)
    {
      CSMovers[ip]->advanceWalkers(W.begin()+wPerNode[ip],W.begin()+wPerNode[ip+1],true);
    //  CSMovers[ip]->updateNorms();
    //  app_log()<<cumnorm<< std::endl;
      if (prestep==nWarmupSteps-1) CSMovers[ip]->updateNorms();
    }
    
 //   app_log()<<"Norms updated\n";
    if (nWarmupSteps && QMCDriverMode[QMC_UPDATE_MODE])
      CSMovers[ip]->updateCSWalkers(W.begin()+wPerNode[ip],W.begin()+wPerNode[ip+1]);
//       }
    
    
  }
  
  for(int ip=0; ip<NumThreads; ++ip)
    wClones[ip]->clearEnsemble();
  if(nSamplesPerThread)
    for(int ip=0; ip<NumThreads; ++ip)
      wClones[ip]->setNumSamples(nSamplesPerThread);
  nWarmupSteps=0;
  
}

}


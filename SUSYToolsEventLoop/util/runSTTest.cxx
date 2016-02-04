#include "xAODRootAccess/Init.h"
#include "SampleHandler/SampleHandler.h"
#include "SampleHandler/Sample.h"
#include "SampleHandler/ScanDir.h"
#include "SampleHandler/ToolsDiscovery.h"
#include "EventLoop/Job.h"
#include "EventLoop/DirectDriver.h"
#include "EventLoop/OutputStream.h"
#include "EventLoopAlgs/NTupleSvc.h"
#include "SampleHandler/DiskListLocal.h"
#include <TSystem.h>
#include <TFile.h>
#include <TStopwatch.h>
#include "SampleHandler/ScanDir.h"
#include "EventLoop/CondorDriver.h"
#include "SUSYTools/SUSYCrossSection.h"
#include "SUSYToolsEventLoop/SUSYToolsTester.h"
#include "xAODCutFlow/CutBookkeeper.h"
#include "xAODCutFlow/CutBookkeeperContainer.h"
#include "xAODRootAccess/TEvent.h"
#include "xAODEventInfo/EventInfo.h"

using namespace std;

int main( int argc, char* argv[] ) 
{

  // Take the submit directory from the input if provided:
  std::string submitDir = "submitDir";
  if( argc > 1 ) submitDir = argv[ 1 ];
  std::string batch = "";
  if( argc > 2 ) batch = argv[ 2 ];
  bool useCondor = batch.compare("batch")==0;
  int nFilesPerWorker = 1;
  if( argc > 3 ) nFilesPerWorker = atoi(argv[ 3 ]);
  
  TStopwatch sw_read;
  TStopwatch sw_proc;
  
  std::string pattern = "";
  if( submitDir.find(",")!=std::string::npos ){
     pattern = submitDir.substr(submitDir.find(",")+1);
     submitDir = submitDir.substr(0,submitDir.find(","));
  }
  else pattern = submitDir;
  std::cout << pattern << std::endl;

  // Set up the job for xAOD access:
  xAOD::Init().ignore();

  // create a new sample handler to describe the data files we use
  SH::SampleHandler sh;

  sw_read.Start();
  SH::DiskListLocal list ("/input");
  SH::ScanDir()
    .filePattern("*06451147._000041*")
    .scan(sh, list);

  sh.setMetaString ("nc_tree", "CollectionTree");
  SUSY::CrossSectionDB myDB("$ROOTCOREBIN/data/SUSYTools/mc15_13TeV/");

  sh.print ();
  sw_read.Stop();

  sw_proc.Start();

  EL::Job job;
  job.sampleHandler (sh);
  job.options()->setDouble (EL::Job::optMaxEvents, 500); 

  SUSYToolsTester *alg = new SUSYToolsTester;
  
  job.algsAdd (alg);

  /*for (auto& sample : sh)
  {    
    uint64_t nEventsProcessed(0)  ;
    double sumOfWeights(0)        ; //!
    double sumOfWeightsSquared(0) ;
    int run(0);
    for (auto& file : sample->makeFileList())
    {
      std::unique_ptr<TFile> f (TFile::Open (file.c_str(), "READ"));
      xAOD::TEvent event (f.get());
      if( !run ){
         const xAOD::EventInfo* eventInfo = 0;
	 event.retrieve( eventInfo, "EventInfo");
	 run = eventInfo->mcChannelNumber();
      }

      //Read the CutBookkeeper container
      const xAOD::CutBookkeeperContainer* completeCBC = 0;
      event.retrieveMetaInput(completeCBC, "CutBookkeepers");

    // Find the smallest cycle number, the original first processing step/cycle
      int minCycle = 10000;
      for ( auto cbk : *completeCBC ) {
	if ( ! cbk->name().empty()  && minCycle > cbk->cycle() ){ minCycle = cbk->cycle(); }
      }

      // Now, find the right one that contains all the needed info...
      const xAOD::CutBookkeeper* allEventsCBK=0;
      for ( auto cbk :  *completeCBC ) {
	if ( minCycle == cbk->cycle() && cbk->name() == "AllExecutedEvents" ){
	  allEventsCBK = cbk;
	  break;
	}
      }

      nEventsProcessed    += allEventsCBK->nAcceptedEvents();
      sumOfWeights        += allEventsCBK->sumOfEventWeights();
      sumOfWeightsSquared += allEventsCBK->sumOfEventWeightsSquared();
      f->Close();
    }
    
    double xs = run>999990? 0.102*860. : 1000.*myDB.xsectTimesEff(run);
    sample->setMetaDouble( "my_xs", xs );
    sample->meta()->setDouble ("my_wevt", sumOfWeights);
    std::cout << run << ": " << sumOfWeights << std::endl;
  }*/

  if( !useCondor ){
    // make the driver we want to use:
    // this one works by running the algorithm directly:
    EL::DirectDriver driver;
    // we can use other drivers to run things on the Grid, with PROOF, etc.

    // process the job using the driver
    driver.submit (job, submitDir);  
  }
  else{
    EL::CondorDriver driver;
    //driver.options()->setString (EL::Job::optSubmitFlags, "-L /bin/bash");
    driver.shellInit = "export ATLAS_LOCAL_ROOT_BASE=/cvmfs/atlas.cern.ch/repo/ATLASLocalRootBase && source ${ATLAS_LOCAL_ROOT_BASE}/user/atlasLocalSetup.sh";
    
    job.options()->setDouble(EL::Job::optFilesPerWorker, nFilesPerWorker);
    job.options()->setString(EL::Job::optCondorConf, "GetEnv = True\n");

    //job.options()->setString (EL::Job::optCondorConf, "parameter = value");
    driver.submit (job, submitDir);
  }
  sw_proc.Stop();
  cout << "Read report:" << endl;
  sw_read.Print();
  cout << "Proc report:" << endl;
  sw_proc.Print();
  //delete alg;
  cerr << "Hello" << endl;
  
  return 0;
}

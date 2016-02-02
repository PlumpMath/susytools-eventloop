#include <EventLoop/Job.h>
#include <EventLoop/StatusCode.h>
#include <EventLoop/Worker.h>
#include <SUSYToolsEventLoop/SUSYToolsTester.h>

// System include(s):
#include <memory>
#include <cstdlib>
// #include <valgrind/callgrind.h>
//#include <gperftools/profiler.h>

// ROOT include(s):
#include <TFile.h>
#include <TError.h>
#include <TString.h>
#include <TStopwatch.h>

// EDM include(s):
#include "xAODEventInfo/EventInfo.h"
#include "xAODMuon/MuonContainer.h"
#include "xAODEgamma/ElectronContainer.h"
#include "xAODEgamma/PhotonContainer.h"
#include "xAODTau/TauJetContainer.h"
#include "xAODTruth/TruthParticleAuxContainer.h"
#include "xAODJet/JetContainer.h"
#include "xAODJet/JetAuxContainer.h"
#include "xAODCaloEvent/CaloCluster.h"
#include "xAODTruth/TruthParticleContainer.h"
#include "xAODTruth/TruthEventContainer.h"
#include "xAODTruth/TruthEvent.h"
#include "xAODCore/ShallowCopy.h"
#include "xAODMissingET/MissingETContainer.h"
#include "xAODMissingET/MissingETAuxContainer.h"
#include "xAODBTaggingEfficiency/BTaggingEfficiencyTool.h"
#include "xAODBase/IParticleHelpers.h"
#include "xAODTruth/xAODTruthHelpers.h"
#include "TauAnalysisTools/TauTruthMatchingTool.h"
#include "GoodRunsLists/GoodRunsListSelectionTool.h"
#include "PileupReweighting/PileupReweightingTool.h"

// Local include(s):
#include "CPAnalysisExamples/errorcheck.h"
#include "SUSYTools/SUSYObjDef_xAOD.h"

// Other includes
#include "PATInterfaces/SystematicVariation.h"
#include "PATInterfaces/SystematicRegistry.h"
#include "PATInterfaces/SystematicCode.h"

#include "METUtilities/METSystematicsTool.h"

#include "xAODCutFlow/CutBookkeeper.h"
#include "xAODCutFlow/CutBookkeeperContainer.h"

const char *cut_name[] =
  {"All",
   "GRL",
   "TileTrip",
   "Trigger",
   "Cosmic veto",
   "==1 baseline lepton",
   "==1 signal lepton",
   "lepton pT>20 GeV",
   "Njet(pT>50 GeV)>=2"
  };

enum sel{
  signallep,
  baseline,
  bad,
  cosmic,
  goodpt,
  btagged,
  trgmatch,
  passOR
};

// this is needed to distribute the algorithm to the workers
ClassImp(SUSYToolsTester)

using namespace std;

//std::unique_ptr<ST::SUSYObjDef_xAOD> objTool;
GoodRunsListSelectionTool *m_grl(0);
ST::SettingDataSource datasource;
int isData = 0;
int isAtlfast = 0;
//  int useLeptonTrigger = -1;
int NoSyst = 1;
int debug = 1;
int JESNPset = 0;
// The application's name:
const char* APP_NAME = "SUSYToolsTester";
std::vector<ST::SystInfo> systInfoList;
std::vector<std::vector<int> > elcuts;
std::vector<std::vector<int> > mucuts;
xAOD::TStore store;

SUSYToolsTester :: SUSYToolsTester ()
{
  // Here you put any code for the base initialization of variables,
  // e.g. initialize all pointers to 0.  Note that you should only put
  // the most basic initialization here, since this method will be
  // called on both the submission and the worker node.  Most of your
  // initialization code will go into histInitialize() and
  // initialize().
}



EL::StatusCode SUSYToolsTester :: setupJob (EL::Job& job)
{
  // Here you put code that sets up the job on the submission object
  // so that it is ready to work with your algorithm, e.g. you can
  // request the D3PDReader service or add output files.  Any code you
  // put here could instead also go into the submission script.  The
  // sole advantage of putting it here is that it gets automatically
  // activated/deactivated when you add/remove the algorithm from your
  // job, which may or may not be of value to you.
 
  // let's initialize the algorithm to use the xAODRootAccess package
  job.useXAOD ();
  xAOD::Init( "SUSYToolsTester" ).ignore(); // call before opening first file

  return EL::StatusCode::SUCCESS;
}



EL::StatusCode SUSYToolsTester :: histInitialize ()
{
  // Here you do everything that needs to be done at the very
  // beginning on each worker node, e.g. create histograms and output
  // trees.  This method gets called before any input files are
  // connected.
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode SUSYToolsTester :: fileExecute ()
{
  // Here you do everything that needs to be done exactly once for every
  // single file, e.g. collect a list of all lumi-blocks processed
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode SUSYToolsTester :: changeInput (bool firstFile)
{
  // Here you do everything you need to do when we change input files,
  // e.g. resetting branch addresses on trees.  If you are using
  // D3PDReader or a similar service this method is not needed.
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode SUSYToolsTester :: initialize ()
{
  // Here you do everything that you need to do after the first input
  // file has been connected and before the first event is processed,
  // e.g. create additional histograms based on which variables are
  // available in the input files.  You can also create all of your
  // histograms and trees in here, but be aware that this method
  // doesn't get called if no events are processed.  So any objects
  // you create here won't be available in the output if you have no
  // input events.

  m_event = wk()->xaodEvent();

  //TODO
  if(isData<0 || isAtlfast<0) {
    Error( APP_NAME, "One of the flags isData or isAtlfast was not set! Must provide isData or isAtlfast." );
    Error( APP_NAME, "  Usage: %s [xAOD file name] [nEvtMax] [isData=0/1 isAtlfast=0/1] [NoSyst] [Debug]", APP_NAME );
    return 10;
  }
  datasource = isData ? ST::Data : (isAtlfast ? ST::AtlfastII : ST::FullSim);

  // GRL tool
  if(isData){
    m_grl = new GoodRunsListSelectionTool("GoodRunsListSelectionTool");
    std::vector<std::string> myvals;
    myvals.push_back("/afs/cern.ch/user/a/atlasdqm/grlgen/All_Good/data15_13TeV.periodAllYear_DetStatus-v69-pro19-03_DQDefects-00-01-02_PHYS_StandardGRL_All_Good_25ns.xml");
    CHECK( m_grl->setProperty( "GoodRunsListVec", myvals) );
    CHECK( m_grl->setProperty("PassThrough", false) );
    CHECK( m_grl->initialize() );
		
    Info( APP_NAME, "GRL tool initialized... " );
  }

  // as a check, let's see the number of events in our xAOD
  Info("initialize()", "Number of events = %lli", m_event->getEntries() ); // print long long int

  // Create the tool(s) to test:
	
  //ST::SUSYObjDef_xAOD objTool("SUSYObjDef_xAOD");
  objTool = unique_ptr<ST::SUSYObjDef_xAOD>(new ST::SUSYObjDef_xAOD("SUSYObjDef_xAOD"));
	
  std::cout << " ABOUT TO INITIALIZE SUSYTOOLS " << std::endl;
	
  ///////////////////////////////////////////////////////////////////////////////////////////
  // Configure the SUSYObjDef instance
  CHECK( objTool->setProperty("DataSource",datasource) ) ;
  // CHECK( objTool->setProperty("ConfigFile", "/analysis/SUSYTools_Default.conf") );

  //  CHECK( objTool->setProperty("UseLeptonTrigger",useLeptonTrigger) );
  /*CHECK( objTool->setProperty("JetInputType", xAOD::JetInput::EMTopo) );
  CHECK( objTool->setProperty("EleId","TightLH") );
  CHECK( objTool->setProperty("EleIdBaseline","LooseLH") );
  CHECK( objTool->setProperty("TauId","Tight") );
  CHECK( objTool->setProperty("EleIsoWP","GradientLoose") );
  CHECK( objTool->setProperty("METDoTrkSyst",true) );
  CHECK( objTool->setProperty("METDoCaloSyst",false) );*/


  ///////////////////////////////////////////////////////////////////////////////////////////
  ////                                                                                   ////
  ////       ****     THESE FILES ARE MEANT FOR EXAMPLES OF USAGE ONLY        ****       ////
  ////       ****        AND SHOULD NOT BE USED FOR SERIOUS ANALYSIS          ****       ////
  ////                                                                                   ////
  ///////////////////////////////////////////////////////////////////////////////////////////
  std::vector<std::string> prw_conf;
  prw_conf.push_back("dev/SUSYTools/merged_prw.root");
  CHECK( objTool->setProperty("PRWConfigFiles", prw_conf) );
  std::vector<std::string> prw_lumicalc;
  prw_lumicalc.push_back("/analysis/ilumicalc_histograms_None_266904-267639.root");
  CHECK( objTool->setProperty("PRWLumiCalcFiles", prw_lumicalc) );
  ///////////////////////////////////////////////////////////////////////////////////////////

 /* CHECK(objTool->setProperty("JESNuisanceParameterSet",JESNPset) );

  if(debug) objTool->msg().setLevel( MSG::VERBOSE );
  CHECK(objTool->setProperty("DebugMode",(bool)debug) );
	
  // if( objTool->SUSYToolsInit().isFailure() ) {
  //   Error( APP_NAME, "Failed to initialise tools in SUSYToolsInit()..." );
  //   Error( APP_NAME, "Exiting..." );
  //   exit(-1);
  // }
	*/
  if( objTool->initialize() != StatusCode::SUCCESS){
    Error( APP_NAME, "Cannot intialize SUSYObjDef_xAOD..." );
    Error( APP_NAME, "Exiting... " );
    exit(-1);
  }else{
    Info( APP_NAME, "SUSYObjDef_xAOD initialized... " );
  }
  std::cout << " INITIALIZED SUSYTOOLS " << std::endl;
	

  // Counter for cuts:
  if(NoSyst) {
    ST::SystInfo infodef;
    infodef.affectsKinematics = false;
    infodef.affectsWeights = false;
    infodef.affectsType = ST::Unknown;
    systInfoList.push_back(infodef);
  } else {
    systInfoList = objTool->getSystInfoList();
  }
	
  size_t isys=0;
  const size_t Ncuts = 9;
  const size_t Nsyst = systInfoList.size();
  for (isys=0; isys<Nsyst; ++isys) {
    std::vector<int> elcutsCurrentSyst;
    std::vector<int> mucutsCurrentSyst;
    for (size_t icut=0; icut<Ncuts; ++icut) {
      elcutsCurrentSyst.push_back(0);
      mucutsCurrentSyst.push_back(0);
    }
    elcuts.push_back(elcutsCurrentSyst);
    mucuts.push_back(elcutsCurrentSyst);
  }

  if (objTool->resetSystematics() != CP::SystematicCode::Ok){
    Error(APP_NAME, "Cannot reset SUSYTools systematics" );
    exit(-2);
  }

  return EL::StatusCode::SUCCESS;
}



EL::StatusCode SUSYToolsTester :: execute ()
{
  // Here you do everything that needs to be done on every single
  // events, e.g. read input variables, apply cuts, and fill
  // histograms and trees.  This is where most of your actual analysis
  // code will go.
    // Tell the object which entry to look at:
//    m_event->getEntry( entry );

    CHECK( objTool->ApplyPRWTool());

    // Print some event information for fun:
    const xAOD::EventInfo* ei = 0;
    CHECK( m_event->retrieve( ei, "EventInfo" ) );
    
    //Needed by muon trigger SF tool
    UInt_t runNumber_forMu = 0;
    if (!isData)
      runNumber_forMu = ei->auxdata<unsigned int>("RandomRunNumber");
    else
      runNumber_forMu = ei->runNumber();
    if (runNumber_forMu == 0) runNumber_forMu = 276073; // CO: why? remove?
    CHECK( objTool->setRunNumber(runNumber_forMu));
       
    // // TauTruth matching
    // const xAOD::TruthParticleContainer* truthP = 0;
    // if (!isData) {
    //   // Get the Truth Particles from the event:
    //   CHECK( event.retrieve( truthP, "TruthParticles" ) );
    //   if(debug) Info( APP_NAME, "Number of truth particles: %i",
    // 		      static_cast< int >( truthP->size() ) );			
    //   CHECK( T2MT.initializeEvent() );
    // }

    // execute the pileup reweighting tool
    // Check if input file is mc14_13TeV to skip pileup reweighting
    // bool mc14_13TeV = false;
    // if( ei->runNumber() == 222222) mc14_13TeV = true;
    // if (!isData && !mc14_13TeV){ // Only reweight 8 TeV MC
    //   CHECK( m_pileupReweightingTool->execute() );
			
    //   if(debug) Info( APP_NAME,"PileupReweightingTool: PileupWeight %f RandomRunNumber %i RandomLumiBlockNumber %i",ei->auxdata< double >("PileupWeight"), ei->auxdata< unsigned int >("RandomRunNumber"),  ei->auxdata< unsigned int >("RandomLumiBlockNumber") );
    // }
		
    bool eventPassesGRL(true);
    bool eventPassesTileTrip(true); // move to xAOD tool!
    bool eventPassesTrigger(true); // coming soon!

    if (isData) {
      eventPassesGRL = m_grl->passRunLB(ei->runNumber(), ei->lumiBlock());
    } else {
      // Get the Truth Event information from the event:
      const xAOD::TruthEventContainer* truthE = 0;
      CHECK( m_event->retrieve( truthE, "TruthEvents" ) );
    }
		
    // Get the nominal object containers from the event
    // Electrons
    xAOD::ElectronContainer* electrons_nominal(0);
    xAOD::ShallowAuxContainer* electrons_nominal_aux(0);
    CHECK( objTool->GetElectrons(electrons_nominal,electrons_nominal_aux) );
		
    // // Photons
    // xAOD::PhotonContainer* photons_nominal(0);
    // xAOD::ShallowAuxContainer* photons_nominal_aux(0);
    // CHECK( objTool->GetPhotons(photons_nominal,photons_nominal_aux) );
		
    // Muons
    xAOD::MuonContainer* muons_nominal(0);
    xAOD::ShallowAuxContainer* muons_nominal_aux(0);
    CHECK( objTool->GetMuons(muons_nominal,muons_nominal_aux) );
		
    // Jets
    xAOD::JetContainer* jets_nominal(0);
    xAOD::ShallowAuxContainer* jets_nominal_aux(0);
    CHECK( objTool->GetJets(jets_nominal,jets_nominal_aux,true) );
		
    // // Taus
    // xAOD::TauJetContainer* taus_nominal(0);
    // xAOD::ShallowAuxContainer* taus_nominal_aux(0);
    // CHECK( objTool->GetTaus(taus_nominal,taus_nominal_aux) );
		
    // MET
    xAOD::MissingETContainer* metcst_nominal = new xAOD::MissingETContainer;
    xAOD::MissingETAuxContainer* metcst_nominal_aux = new xAOD::MissingETAuxContainer;
    metcst_nominal->setStore(metcst_nominal_aux);
    metcst_nominal->reserve(10);
		
    xAOD::MissingETContainer* mettst_nominal = new xAOD::MissingETContainer;
    xAOD::MissingETAuxContainer* mettst_nominal_aux = new xAOD::MissingETAuxContainer;
    mettst_nominal->setStore(mettst_nominal_aux);
    mettst_nominal->reserve(10);
		
    // Set up the event weights
    // Base should include all weights that do not depend on individual objects
    double base_event_weight(1.);
    if(!isData) base_event_weight *= ei->mcEventWeight();
		
    // Additionally define a nominal weight for each object type
    double elecSF_nominal(1.);
    double muonSF_nominal(1.);
    double btagSF_nominal(1.);
		
    bool isNominal(true);
    size_t isys=0;
    // Now loop over all the systematic variations
    for(const auto& sysInfo : systInfoList){
      const CP::SystematicSet& sys = sysInfo.systset;
      // std::cout << ">>>> Working on variation: \"" <<(sys.name()).c_str() << "\" <<<<<<" << std::endl;
			
      size_t icut = 0;
      // log all events
      elcuts[isys][icut] += 1;
      mucuts[isys][icut] += 1;
      ++icut;
			
      // does the event pass the GRL?
      if (!eventPassesGRL) {++isys; continue;}
      elcuts[isys][icut] += 1;
      mucuts[isys][icut] += 1;
      ++icut;
			
      // Apply TileTrip
      if (!eventPassesTileTrip)	{++isys; continue;}
      elcuts[isys][icut] += 1;
      mucuts[isys][icut] += 1;
      ++icut;

      if(debug) {
	// Testing trigger
	std::string trigItem[6]={"HLT_e26_lhtight_iloose", "HLT_e60_lhmedium", 
				 "HLT_mu26_imedium", "HLT_mu50",
				 "HLT_xe100", "HLT_noalg_.*"};
	for(int it=0; it<6; it++){
	  bool passed = objTool->IsTrigPassed(trigItem[it]);
	  float prescale = objTool->GetTrigPrescale(trigItem[it]);
	  Info( APP_NAME, "passing %s trigger? %d, prescale %f", trigItem[it].c_str(), (int)passed, prescale );
	  // example of more sophisticated trigger access
	  const Trig::ChainGroup* cg = objTool->GetTrigChainGroup(trigItem[it]);
	  bool cg_passed = cg->isPassed();
	  float cg_prescale = cg->getPrescale();
	  Info( APP_NAME, "ChainGroup %s: passing trigger? %d, prescale %f", trigItem[it].c_str(), (int)cg_passed, cg_prescale );
	  for(const auto& cg_trig : cg->getListOfTriggers()) {
	    Info( APP_NAME, "               includes trigger %s", cg_trig.c_str() );
	  }
	}
      }
			
      // Trigger (coming soon...)
      if (!eventPassesTrigger)	{++isys; continue;}
      elcuts[isys][icut] += 1;
      mucuts[isys][icut] += 1;
      ++icut;
			
      // Generic pointers for either nominal or systematics copy
      xAOD::ElectronContainer* electrons(electrons_nominal);
      // xAOD::PhotonContainer* photons(photons_nominal);
      xAOD::MuonContainer* muons(muons_nominal);
      xAOD::JetContainer* jets(jets_nominal);
      // xAOD::TauJetContainer* taus(taus_nominal);
      xAOD::MissingETContainer* metcst(metcst_nominal);
      xAOD::MissingETContainer* mettst(mettst_nominal);
      // Aux containers too
      xAOD::ShallowAuxContainer* electrons_aux(electrons_nominal_aux);
      // xAOD::ShallowAuxContainer* photons_aux(photons_nominal_aux);
      xAOD::ShallowAuxContainer* muons_aux(muons_nominal_aux);
      xAOD::ShallowAuxContainer* jets_aux(jets_nominal_aux);
      // xAOD::ShallowAuxContainer* taus_aux(taus_nominal_aux);
      xAOD::MissingETAuxContainer* metcst_aux(metcst_nominal_aux);
      xAOD::MissingETAuxContainer* mettst_aux(mettst_nominal_aux);
			
      xAOD::JetContainer* goodJets = new xAOD::JetContainer(SG::VIEW_ELEMENTS);
      CHECK( store.record(goodJets, "MySelJets"+sys.name()) );
			
      // Tell the SUSYObjDef_xAOD which variation to apply
      if (objTool->applySystematicVariation(sys) != CP::SystematicCode::Ok){
	Error(APP_NAME, "Cannot configure SUSYTools for systematic var. %s", (sys.name()).c_str() );
      }else{
	if(debug) Info(APP_NAME, "Variation \"%s\" configured...", (sys.name()).c_str() );
      }
      if(sysInfo.affectsKinematics || sysInfo.affectsWeights) isNominal = false;
			
      // If nominal, compute the nominal weight, otherwise recompute the weight
      double event_weight = base_event_weight;
			
      // If necessary (kinematics affected), make a shallow copy with the variation applied
      bool syst_affectsElectrons = ST::testAffectsObject(xAOD::Type::Electron, sysInfo.affectsType);
      bool syst_affectsMuons = ST::testAffectsObject(xAOD::Type::Muon, sysInfo.affectsType);
      // bool syst_affectsTaus = ST::testAffectsObject(xAOD::Type::Tau, sysInfo.affectsType);
      // bool syst_affectsPhotons = ST::testAffectsObject(xAOD::Type::Photon, sysInfo.affectsType);
      bool syst_affectsJets = ST::testAffectsObject(xAOD::Type::Jet, sysInfo.affectsType);
      bool syst_affectsBTag = ST::testAffectsObject(xAOD::Type::BTag, sysInfo.affectsType);
      //      bool syst_affectsMET = ST::testAffectsObject(xAOD::Type::MissingET, sysInfo.affectsType);
			
      if(sysInfo.affectsKinematics) {
	if(syst_affectsElectrons) {
	  xAOD::ElectronContainer* electrons_syst(0);
	  xAOD::ShallowAuxContainer* electrons_syst_aux(0);
	  CHECK( objTool->GetElectrons(electrons_syst,electrons_syst_aux) );
	  electrons = electrons_syst;
	  electrons_aux = electrons_syst_aux;
	}
				
	if(syst_affectsMuons) {
	  xAOD::MuonContainer* muons_syst(0);
	  xAOD::ShallowAuxContainer* muons_syst_aux(0);
	  CHECK( objTool->GetMuons(muons_syst,muons_syst_aux) );
	  muons = muons_syst;
	  muons_aux = muons_syst_aux;
	}
				
	// if(syst_affectsTaus) {
	//   xAOD::TauJetContainer* taus_syst(0);
	//   xAOD::ShallowAuxContainer* taus_syst_aux(0);
	//   CHECK( objTool->GetTaus(taus_syst,taus_syst_aux) );
	//   taus = taus_syst;
	//   taus_aux = taus_syst_aux;
	// }
				
	// if(syst_affectsPhotons) {
	//   xAOD::PhotonContainer* photons_syst(0);
	//   xAOD::ShallowAuxContainer* photons_syst_aux(0);
	//   CHECK( objTool->GetPhotons(photons_syst,photons_syst_aux) );
	//   photons = photons_syst;
	//   photons_aux = photons_syst_aux;
	// }
				
	if(syst_affectsJets) {
	  xAOD::JetContainer* jets_syst(0);
	  xAOD::ShallowAuxContainer* jets_syst_aux(0);
	  CHECK( objTool->GetJetsSyst(*jets_nominal,jets_syst,jets_syst_aux) );
	  jets = jets_syst;
	  jets_aux = jets_syst_aux;
	}
				
	xAOD::MissingETContainer* metcst_syst = new xAOD::MissingETContainer;
	xAOD::MissingETAuxContainer* metcst_syst_aux = new xAOD::MissingETAuxContainer;
	xAOD::MissingETContainer* mettst_syst = new xAOD::MissingETContainer;
	xAOD::MissingETAuxContainer* mettst_syst_aux = new xAOD::MissingETAuxContainer;
	metcst_syst->setStore(metcst_syst_aux);
	mettst_syst->setStore(mettst_syst_aux);
	metcst_nominal->reserve(10);
	metcst_nominal->reserve(10);

	metcst = metcst_syst;
	mettst = mettst_syst;
	metcst_aux = metcst_syst_aux;
	mettst_aux = mettst_syst_aux;
      }
			
      // ***************    ************************    *****************
      // *************** Now start processing the event *****************
      // ***************    ************************    *****************
			
      if(isNominal || (sysInfo.affectsKinematics && syst_affectsElectrons) ) {
	for(const auto& el : *electrons) {
	  //objTool->IsSignalElectron( *el ) ;
					
	  if(!isData){
	    const xAOD::TruthParticle* truthEle = xAOD::TruthHelpers::getTruthParticle(*el);
	    if(truthEle) {if(debug) Info( APP_NAME, " Truth Electron pt %f eta %f", truthEle->pt(), truthEle->eta() );}
	    else {if(debug) Info( APP_NAME, " Truth Electron not found" );}
	  }
	}
      }
			
      if(isNominal || (sysInfo.affectsKinematics && syst_affectsMuons) ) {
	for(const auto& mu : *muons) {
	  //objTool->IsSignalMuon( *mu ) ;
	  //objTool->IsCosmicMuon( *mu ) ;
					
	  if(!isData){
	    // Example to access MC type/origin
	    int muonTruthType = 0;
	    int muonTruthOrigin = 0;
	    const xAOD::TrackParticle* trackParticle = mu->primaryTrackParticle();
	    if (trackParticle) {
	      static SG::AuxElement::Accessor<int> acc_truthType("truthType");
	      static SG::AuxElement::Accessor<int> acc_truthOrigin("truthOrigin");
	      if (acc_truthType.isAvailable(*trackParticle)  ) muonTruthType   = acc_truthType(*trackParticle);
	      if (acc_truthOrigin.isAvailable(*trackParticle)) muonTruthOrigin = acc_truthOrigin(*trackParticle);
	      const xAOD::TruthParticle* truthMu = xAOD::TruthHelpers::getTruthParticle(*trackParticle);
	      if(truthMu)
		{if(debug) Info( APP_NAME, " Truth Muon pt %f eta %f, type %d, origin %d",
				 truthMu->pt(), truthMu->eta(), muonTruthType, muonTruthOrigin );}
	      else
		{if(debug) Info( APP_NAME, " Truth Muon not found");}
	    }
	  }
	}
      }

      if(isNominal || (sysInfo.affectsKinematics && syst_affectsJets)) {
	for(const auto& jet : *jets) {
	  objTool->IsBJet( *jet) ;
	}
      }
	
      // if(debug) Info(APP_NAME, "Tau truth");
      // if(isNominal) {
      // 	for(const auto& tau : *taus){
      // 	  if (!isData){
      // 	    const xAOD::TruthParticle* truthTau = T2MT.applyTruthMatch(*tau) ;
      // 	    if(tau->auxdata<char>("IsTruthMatched") || !truthTau){
      // 	      if(debug) Info( APP_NAME,
      // 			      "Tau was matched to a truth tau, which has %i prongs and a charge of %i",
      // 			      int(tau->auxdata<size_t>("TruthProng")),
      // 			      tau->auxdata<int>("TruthCharge"));
      // 	    } else {
      // 	      if(debug) Info( APP_NAME, "Tau was not matched to truth" );
      // 	    }
      // 	  }
      // 	}
      // }

      if(debug) Info(APP_NAME, "Overlap removal");
			
      // do overlap removal
      if(isNominal || (sysInfo.affectsKinematics && (syst_affectsElectrons || syst_affectsMuons || syst_affectsJets))) {
	CHECK( objTool->OverlapRemoval(electrons, muons, jets) );
      }
			
      if(debug) Info(APP_NAME, "GoodJets?");
      for(const auto& jet : *jets) {
	if( jet->auxdata<char>("baseline")==1  &&
	    jet->auxdata<char>("passOR")==1  &&
	    jet->pt() > 20000.  && ( fabs( jet->eta()) < 2.5) ) {
	  goodJets->push_back(jet);
	}
      }
      
      if(isNominal || sysInfo.affectsKinematics) {
      	CHECK( objTool->GetMET(*metcst,
      			      jets,
      			      electrons,
      			      muons,
      			      0, // photons,
      			      0, // taus
			      false) );
      	if(debug) Info(APP_NAME, "METTST?");
      	CHECK( objTool->GetMET(*mettst,
      			      jets,
      			      electrons,
      			      muons,
      			      0, // photons,
			      0, // taus,
			      true) );
      	if(debug) Info(APP_NAME, "MET done");
      }
      
      float elecSF = 1.0;
      int el_idx[8] = {0};
      for(const auto& el : *electrons) {
	if( el->auxdata<char>("passOR") ==0  ) {
	  el_idx[passOR]++;
	  continue;
	}
	if( el->auxdata<char>("baseline") ==1  )
	  el_idx[baseline]++;
	if( el->auxdata<char>("signal") ==1  ) {
	  el_idx[signallep]++;
	  if( el->pt() > 20000. ) {
	    el_idx[goodpt]++;
	  }
	}
      }
      if(isNominal || syst_affectsElectrons) {
	elecSF = objTool->GetTotalElectronSF(*electrons);
      }
      if(isNominal) {elecSF_nominal = elecSF;}
      else if(!syst_affectsElectrons) {elecSF = elecSF_nominal;}
      event_weight *= elecSF;
			
      float muonSF = 1.0;
      int mu_idx[8]={0};
      for(const auto& mu : *muons) {
	if( mu->auxdata<char>("passOR") == 0  ) {
	  mu_idx[passOR]++;
	  continue;
	}
	if( mu->auxdata<char>("baseline") == 1  ) {
	  mu_idx[baseline]++;
	  if( mu->auxdata<char>("cosmic") == 1  ) {
	    mu_idx[cosmic]++;
	  }
	}
	if( mu->auxdata<char>("signal") == 1  ) {
	  mu_idx[signallep]++;
	  if( mu->pt() > 20000. )  {
	    mu_idx[goodpt]++;
	  }
	}
      }

      if(isNominal || syst_affectsMuons) {
	muonSF = objTool->GetTotalMuonSF(*muons,true,true);
      }
			
      if(isNominal) {muonSF_nominal = muonSF;}
      else if(!syst_affectsMuons) {muonSF = muonSF_nominal;}
      event_weight *= muonSF;
			
      int jet_idx[8]={0};
      for(const auto& jet : *goodJets) {
	if( jet->auxdata<char>("bad") == 1  )
	  jet_idx[bad]++;
	if( jet->auxdata<char>("passOR") == 0  ) {
	  jet_idx[passOR]++;
	  continue;
	}
	if( jet->auxdata<char>("baseline") == 1  ) {
	  jet_idx[baseline]++;
	  if( jet->pt() > 50000. )
	    jet_idx[goodpt]++;
	}
	if( jet->auxdata<char>("bjet") ==1  )
	  jet_idx[btagged]++;
      }
      
      // compute b-tagging SF
      float btagSF(1.);
      if(isNominal) {btagSF = btagSF_nominal = objTool->BtagSF(jets);}
      else if(syst_affectsBTag || (sysInfo.affectsKinematics && syst_affectsJets)) {btagSF = objTool->BtagSF(jets);}
      else {btagSF = btagSF_nominal;}
      event_weight *= btagSF;
			
      // Cosmics
      if(mu_idx[cosmic]==0 ) {
	elcuts[isys][icut] += 1;
	mucuts[isys][icut] += 1;
	++icut;
			
	bool passlep = (el_idx[baseline]+mu_idx[baseline]) == 1;
	if(passlep) {
	  bool passel = el_idx[baseline] == 1;
	  bool passmu = mu_idx[baseline] == 1;
	  if(passel) elcuts[isys][icut] += 1;
	  if(passmu) mucuts[isys][icut] += 1;
	  ++icut;
			
	  passel = el_idx[signallep] == 1;
	  passmu = mu_idx[signallep] == 1;
	  if(passel) elcuts[isys][icut] += 1;
	  if(passmu) mucuts[isys][icut] += 1;
	  ++icut;
			
	  passel = el_idx[goodpt] == 1;
	  passmu = mu_idx[goodpt] == 1;
	  if(passel) elcuts[isys][icut] += 1;
	  if(passmu) mucuts[isys][icut] += 1;
	  ++icut;
	
	  if(jet_idx[goodpt]>=2) {
	    if(passel) elcuts[isys][icut] += 1;
	    if(passmu) mucuts[isys][icut] += 1;
	    ++icut;
	  } // good jets
	} // passlep
      }	// cosmics
			
      // Clean up the systematics copies
      if(sysInfo.affectsKinematics) {
	if(syst_affectsElectrons) {
	  delete electrons;
	  delete electrons_aux;
	}
	if(syst_affectsMuons) {
	  delete muons;
	  delete muons_aux;
	}
	// if(syst_affectsTaus) {
	//   delete taus;
	//   delete taus_aux;
	// }
	// if(syst_affectsPhotons) {
	//   delete photons;
	//   delete photons_aux;
	// }
	if(syst_affectsJets) {
	  delete jets;
	  delete jets_aux;
	}
	delete metcst;
	delete metcst_aux;
	delete mettst;
	delete mettst_aux;
      }
			
      isNominal = false;
      // std::cout << ">>>> Finished with variation: \"" <<(sys.name()).c_str() << "\" <<<<<<" << std::endl;
      ++isys;
    }
		
    // The containers created by the shallow copy are owned by you. Remember to delete them
    // delete jets_nominal; // not these, we put them in the store!
    // delete jets_nominal_aux;
    // delete taus_nominal;
    // delete taus_nominal_aux;
    delete muons_nominal;
    delete muons_nominal_aux;
    delete electrons_nominal;
    delete electrons_nominal_aux;
    // delete photons_nominal;
    // delete photons_nominal_aux;
    delete metcst_nominal;
    delete metcst_nominal_aux;
    delete mettst_nominal;
    delete mettst_nominal_aux;
		
    // store.print();
    store.clear();
  		
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode SUSYToolsTester :: postExecute ()
{
  // Here you do everything that needs to be done after the main event
  // processing.  This is typically very rare, particularly in user
  // code.  It is mainly used in implementing the NTupleSvc.
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode SUSYToolsTester :: finalize ()
{
  // This method is the mirror image of initialize(), meaning it gets
  // called after the last event has been processed on the worker node
  // and allows you to finish up any objects you created in
  // initialize() before they are written to disk.  This is actually
  // fairly rare, since this happens separately for each worker node.
  // Most of the time you want to do your post-processing on the
  // submission node after all your histogram outputs have been
  // merged.  This is different from histFinalize() in that it only
  // gets called on worker nodes that processed input events.
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode SUSYToolsTester :: histFinalize ()
{
  // This method is the mirror image of histInitialize(), meaning it
  // gets called after the last event has been processed on the worker
  // node and allows you to finish up any objects you created in
  // histInitialize() before they are written to disk.  This is
  // actually fairly rare, since this happens separately for each
  // worker node.  Most of the time you want to do your
  // post-processing on the submission node after all your histogram
  // outputs have been merged.  This is different from finalize() in
  // that it gets called on all worker nodes regardless of whether
  // they processed input events.
  return EL::StatusCode::SUCCESS;
}

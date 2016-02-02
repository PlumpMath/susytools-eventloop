#ifndef SUSYToolsEventLoop_SUSYToolsTester_H
#define SUSYToolsEventLoop_SUSYToolsTester_H

#include <EventLoop/Algorithm.h>

// Infrastructure include(s):
#include "xAODRootAccess/Init.h"
#include "xAODRootAccess/TEvent.h"
#include "SUSYTools/SUSYObjDef_xAOD.h"

class SUSYToolsTester : public EL::Algorithm
{
  // put your configuration variables here as public variables.
  // that way they can be set directly from CINT and python.
public:
  // float cutValue;
  std::unique_ptr<ST::SUSYObjDef_xAOD> objTool; //!

    /// description: the event we are reading from
  private:
    xAOD::TEvent *m_event; //!

  // variables that don't get filled at submission time should be
  // protected from being send from the submission node to the worker
  // node (done by the //!)
public:
  // Tree *myTree; //!
  // TH1 *myHist; //!



  // this is a standard constructor
  SUSYToolsTester ();

  // these are the functions inherited from Algorithm
  virtual EL::StatusCode setupJob (EL::Job& job);
  virtual EL::StatusCode fileExecute ();
  virtual EL::StatusCode histInitialize ();
  virtual EL::StatusCode changeInput (bool firstFile);
  virtual EL::StatusCode initialize ();
  virtual EL::StatusCode execute ();
  virtual EL::StatusCode postExecute ();
  virtual EL::StatusCode finalize ();
  virtual EL::StatusCode histFinalize ();

  // this is needed to distribute the algorithm to the workers
  ClassDef(SUSYToolsTester, 1);
};

#endif

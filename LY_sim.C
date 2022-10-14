// Adapted from PandaRoot/tutorials/thailand2017/tut_sim.C

void LY_sim(Int_t nEvents = 100, TString prefix = "dummy", TString inputGenerator = "box:type(13,1):p(0.080):tht(0):phi(0):xyz(66.952,3.2,3.8)", Double_t BeamMomentum = 1,
            TString SimEngine = "TGeant4")
{
  TString parAsciiFile = "all.par";

  auto logger = FairLogger::GetLogger();
  logger->SetLogToScreen(true);
  logger->SetLogScreenLevel("info");

  // -----   Create the Simulation run manager ------------------------------
  PndMasterRunSim *fRun = new PndMasterRunSim();
  fRun->SetInput(inputGenerator);
  fRun->SetName(SimEngine);
  fRun->SetParamAsciiFile(parAsciiFile);
  fRun->SetNumberOfEvents(nEvents);
  fRun->SetBeamMom(BeamMomentum);
  fRun->SetStoreTraj();
  // -----  Initialization   ------------------------------------------------
  fRun->Setup(prefix);
  // -----   Geometry   -----------------------------------------------------
  fRun->CreateGeometry(); // Default
  // -----   Event generator   ----------------------------------------------
  fRun->SetGenerator();
  // -----   Add tasks   ----------------------------------------------------
  fRun->AddSimTasks();
  // -----   Intialise and run   --------------------------------------------
  fRun->Init();
  fRun->Run(nEvents);
  fRun->Finish();
}

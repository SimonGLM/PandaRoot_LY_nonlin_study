// Adapted from PandaRoot/tutorials/thailand2017/tut_aod.C

void LY_digi(Int_t nEvents = 0, TString prefix = "dummy")
{
  TString parAsciiFile = "all.par";
  PndMasterRunAna *fRun = new PndMasterRunAna();
  fRun->SetInput("dummy");
  fRun->SetOutput("pid");
  fRun->SetParamAsciiFile(parAsciiFile);
  fRun->Setup(prefix);
  fRun->AddDigiTasks();
  fRun->Init();
  fRun->Run(0, nEvents);
  fRun->Finish();
}

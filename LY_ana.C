#include "FairTask.h"
#include <chrono>  // chrono::system_clock
#include <ctime>   // localtime
#include <sstream> // stringstream
#include <iomanip> // put_time
#include <string>  // string

std::string fullname;
std::string name;

std::string now_YYYY_MM_DD_HH_mm_ss()
{
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
  return ss.str();
}

void saveAs(TNamed *object, TString name)
{
  TCanvas *c = new TCanvas();
  c->SetWindowSize(2000, 1500);
  object->Draw();
  TImage *img = TImage::Create();
  img->FromPad(c);
  img->WriteImage(name);
  delete c;
  delete img;
}

class mytask : public FairTask
{
  FairRootManager *ioman;
  FairRuntimeDb *db;
  FairRun *run;
  PndEmcDigiNonuniformityPar *fNonuniformityPar;

  FILE *logfile;
  TClonesArray *DigiArray;
  TClonesArray *MCArray;
  TClonesArray *BumpArray;
  TDatabasePDG *PdgDB = new TDatabasePDG();
  Int_t nAccepted = 0;
  Int_t nTotal = 0;
  Int_t nEvts = 0;

  Double_t fDetectedPhotonsPerMeV;
  Double_t fSensitiveAreaAPD;
  Double_t fQuantumEfficiencyAPD;
  Double_t fNPhotoElectronsPerMeVAPDBarrel;
  Int_t fGetUse_nonuniformity;

  TH1F *temp = new TH1F("temp", "temporary histo for hit pos", 500, 50, 100);
  TH1F *histoX_hit = new TH1F("X_hit", "position on long crystal axis (far -> close);X [cm];a.u.", 500, 50, 100);
  TH1F *histoE_digi = new TH1F("E_digi", "energy in digitized hit E_{digi};E_{digi} [GeV];cts", 1000, 0, 0.2);
  TH1F *histoR_phe = new TH1F("R_phe", "Lightyield R_{phe};R_{phe} [\\frac{phe}{MeV}];cts", 1000, 0, 300000);
  TH1F *histoE_in = new TH1F("E_in", "total energy of entering MCTracks E_{in};E_{in} [GeV];cts", 1000, 0, 0.5);
  TH1F *histoE_out = new TH1F("E_out", "total energy of exiting MCTracks E_{out};E_{out} [GeV];cts", 1000, 0, 0.5);
  TH1F *histoE_dep = new TH1F("E_dep", "deposited energy E_{dep}=E_{in} - E_{out};E_{dep} [GeV];cts", 1000, 0, 0.2);
  TH1F *histoR_LY = new TH1F("R_LY", "R_{LY} = E_{digi} / E_{dep};R_{LY};cts", 200, 0, 2);
  TH1F *histoN_hits = new TH1F("N_hits", "N_hits per digi", 10, 0, 10);
  TH2I *histoParticlesEntering = new TH2I("N_particles_in", "Number of particles entering per event", 1, 0, 1, (20 - 0) + 1, 0, 20);
  TH2I *histoParticlesExiting = new TH2I("N_particles_out", "Number of particles entering per event", 1, 0, 1, (20 - 0) + 1, 0, 20);
  TH1I *histoEventFilter = new TH1I("N_filter_rejections", "Number of particles rejected per filter", (1 - 0) + 1, 0, 1);

  const Double_t windowR_LY = 0.075;
  const Double_t windowR_phe = 7e3;

  std::map<int, int> globalParticleCountEntering;
  std::map<int, int> globalParticleCountExiting;
  std::map<std::string, int> eventFilterRejections = {{"multiple-entering", 0},
                                                      {"multiple-hits-in-digi", 0},
                                                      {"negative-E_dep", 0},
                                                      {"no-entering", 0},
                                                      {"no-hit-in-digi", 0},
                                                      {"no-primary", 0},
                                                      {"not-in-114crystal", 0},
                                                      {"primary-exiting", 0}};

  InitStatus Init()
  {
    ioman = FairRootManager::Instance();
    if (!ioman)
    {
      LOG(error) << "RootManager not instantiated!";
      return kERROR;
    }

    DigiArray = (TClonesArray *)ioman->GetObject("EmcDigi");
    if (!DigiArray)
    {
      LOG(error) << "No EmcDigi!";
      return kERROR;
    }

    MCArray = (TClonesArray *)ioman->GetObject("MCTrack");
    if (!MCArray)
    {
      LOG(error) << "No MCTrack!";
      return kERROR;
    }

    // Get run and runtime database
    run = FairRun::Instance();
    if (!run)
      Fatal("SetParContainers", "No analysis run");
    db = run->GetRuntimeDb();
    if (!db)
      Fatal("SetParContainers", "No runtime database");

    PndEmcDigiPar *fDigiPar = (PndEmcDigiPar *)db->getContainer("PndEmcDigiPar");
    fDetectedPhotonsPerMeV = fDigiPar->GetDetectedPhotonsPerMeV();
    fSensitiveAreaAPD = fDigiPar->GetSensitiveAreaAPD();
    fQuantumEfficiencyAPD = fDigiPar->GetQuantumEfficiencyAPD();
    fNPhotoElectronsPerMeVAPDBarrel = fDetectedPhotonsPerMeV * fSensitiveAreaAPD / 745. * fQuantumEfficiencyAPD;
    fGetUse_nonuniformity = fDigiPar->GetUse_nonuniformity();

    LOG(detail) << "===== EMC digitization parameters =====";
    LOG(detail) << std::setw(35) << "getUse_nonuniformity = " << fGetUse_nonuniformity;
    LOG(detail) << std::setw(35) << "fQuantumEfficiencyAPD = " << fQuantumEfficiencyAPD;
    LOG(detail) << std::setw(35) << "fDetectedPhotonsPerMeV = " << fDetectedPhotonsPerMeV;
    LOG(detail) << std::setw(35) << "fSensitiveAreaAPD = " << fSensitiveAreaAPD;
    LOG(detail) << std::setw(35) << "fNPhotoElectronsPerMeVAPDBarrel = " << fNPhotoElectronsPerMeVAPDBarrel;
    LOG(detail) << "=======================================";

    fNonuniformityPar = (PndEmcDigiNonuniformityPar *)db->getContainer("PndEmcDigiNonuniformityPar");
    fNonuniformityPar->setChanged();
    fNonuniformityPar->setInputVersion(run->GetRunId(), 1);
    return kSUCCESS;
  }

  void Exec(Option_t *)
  {
    // Simon
    nEvts++;
    Double_t E_digi = 0;
    Double_t E_in = 0;
    Double_t E_out = 0;
    Double_t N_phe = 0;

    std::map<int, int> localParticleCountEntering;
    std::map<int, int> localParticleCountExiting;

    // std::cout << "DigiArray: " << DigiArray->GetEntriesFast() << std::endl;
    // std::cout << "MCArray: " << MCArray->GetEntriesFast() << std::endl;
    // std::cout << "DigiArray: " << DigiArray->GetEntriesFast() << std::endl;
    LOG(detail) << "Event: " << nEvts;
    for (Int_t iDigi = 0; iDigi < DigiArray->GetEntriesFast(); ++iDigi) // why not start at 0??
    {
      nTotal++;
      LOG(detail) << "\e[01;34mDigi " << iDigi + 1 << " of " << DigiArray->GetEntriesFast() << "\e[0m";
      PndEmcDigi *theDigi = (PndEmcDigi *)DigiArray->At(iDigi); // get the digi
      Int_t fDetectorID = theDigi->GetModule() * 1e8 + theDigi->GetRow() * 1e6 + theDigi->GetCopy() * 1e4 + theDigi->GetCrystal();

      //////// FILTER - 114 Crystal ////////
      if (theDigi->GetModule() != 1 || theDigi->GetRow() != 1 || theDigi->GetCopy() != 13 || theDigi->GetCrystal() != 4)
      { // Discard if not in Crystal (1,1,4)
        eventFilterRejections["not-in-114crystal"] += 1;
        LOG(detail) << "  fDetectorID: "
                    << " M: " << theDigi->GetModule() << " R: " << theDigi->GetRow() << " D:" << theDigi->GetCopy() << " C:" << theDigi->GetCrystal() << " (" << fDetectorID << ")";
        LOG(detail) << "  \e[31mRejected.\e[0m Hit is not in [1,1,13,4] crystal";
        continue; // skip event if digi not in (1,1,4)
      }
      LOG(detail) << "  \e[32mPassed.\e[0m Hit is in [1,1,13,4] crystal";
      //////////////////////////////////////

      E_digi = theDigi->GetEnergy();

      // // Nonsensical until Parameters are properly loaded/initialized
      // Double_t c[] = {0, 0, 0};
      // fNonuniformityPar->GetNonuniformityParameters(fDetectorID, c);
      // LOG(detail) << "    fNonuniformityPar={" << c[0] << ", " << c[1] << ", " << c[2] << "}";

      auto hitLinks = theDigi->GetLinksWithType(ioman->GetBranchId("EmcHit")); // hits linked to the digi
      histoN_hits->Fill(hitLinks.GetNLinks());                                 // fill histo

      PndEmcHit *theHit = (PndEmcHit *)ioman->GetCloneOfLinkData(hitLinks.GetLink(0)); // get the hit

      //////// FILTER - Hits in digi ////////
      if (hitLinks.GetNLinks() > 1)
      {
        eventFilterRejections["multiple-hits-in-digi"] += 1;
        LOG(detail) << "  \e[31mRejected.\e[0m Digi consists of multiple Hits";
        LOG(detail) << "    Multiplicity: " << hitLinks.GetNLinks();
        continue; // skip digi if more than one hit in digi
      }
      if (theHit == nullptr)
      {
        histoEventFilter->Fill("no-hit-for-digi", 1);
        eventFilterRejections["no-hit-in-digi"] += 1;
        LOG(detail) << "  \e[31mRejected.\e[0m No Hit in Digi.";
        continue; // skip digi if hit is nullptr
      }
      LOG(detail) << "  \e[32mPassed.\e[0m Digi has only one Hit";
      ///////////////////////////////////////

      // Entering Tracks
      bool PRIMARY_ENTERING = false;
      bool PRIMARY_EXITING = false;
      FairMultiLinkedData enteringLinks = theHit->GetTrackEntering();
      for (Int_t j = 0; j < enteringLinks.GetNLinks(); j++)
      {
        PndMCTrack *enteringTrack = (PndMCTrack *)ioman->GetCloneOfLinkData(enteringLinks.GetLink(j)); // get the Track
        if (enteringTrack != nullptr)
        {
          int pdg = enteringTrack->GetPdgCode();
          localParticleCountEntering[pdg] += 1;
          E_in += (enteringTrack->Get4Momentum())[3];
          PRIMARY_ENTERING |= enteringTrack->IsGeneratorCreated();
          temp->Fill(enteringTrack->GetStartVertex().x()); // Get X value of generated particle, this is only a workaround...
        }
      }

      // Exiting Tracks
      auto exitingLinks = theHit->GetTrackExiting();
      for (Int_t j = 0; j < exitingLinks.GetNLinks(); j++)
      {
        PndMCTrack *exitingTrack = (PndMCTrack *)ioman->GetCloneOfLinkData(exitingLinks.GetLink(j)); // get the Track
        if (exitingTrack != nullptr)
        {
          int pdg = exitingTrack->GetPdgCode();
          localParticleCountExiting[pdg] += 1;
          E_out += (exitingTrack->Get4Momentum())[3];
          PRIMARY_EXITING |= exitingTrack->IsGeneratorCreated();
        }
      }

      //////// FILTER - Primary ////////
      if (!PRIMARY_ENTERING)
      {
        histoEventFilter->Fill("no-primary", 1);
        eventFilterRejections["no-primary"] += 1;
        LOG(detail) << "  \e[31mRejected.\e[0m No entering particle was primary.";
        continue;
      }
      LOG(detail) << "  \e[32mPassed.\e[0m Primary particle is entering";
      if (PRIMARY_EXITING) // Only for particles expected to decay in the crystal
      {
        eventFilterRejections["primary-exiting"] += 1;
        LOG(detail) << "  \e[31mRejected.\e[0m Primary particle is exiting.";
        continue;
      }
      LOG(detail) << "  \e[32mPassed.\e[0m Primary particle is not exiting.";
      //////////////////////////////////

      //////// FILTER - Tracks in hit ////////
      if (enteringLinks.GetNLinks() == 0) // localParticleCountEntering.size() > 1
      {                                   // Exclude hits without entering particles
        histoEventFilter->Fill("no-entering", 1);
        eventFilterRejections["no-entering"] += 1;
        LOG(detail) << "  \e[31mRejected.\e[0m No entering tracks in this hit.";
        continue;
      }
      if (enteringLinks.GetNLinks() > 1)
      { // Count hits with > 1 entering particles
        histoEventFilter->Fill("multiple-entering", 1);
        eventFilterRejections["multiple-entering"] += 1;
        LOG(detail) << "  \e[31mRejected.\e[0m More than one entering track in this hit.";
        continue;
      }
      LOG(detail) << "  \e[32mPassed.\e[0m Only one particle is entering";
      ////////////////////////////////////////

      //////// FILTER - Negative energy deposit ////////
      if ((E_in - E_out) <= 0)
      { // Exclude hits with negative energy deposit
        histoEventFilter->Fill("negative-E_dep", 1);
        eventFilterRejections["negative-E_dep"] += 1;
        LOG(detail) << "  \e[31mRejected.\e[0m Negative energy deposited in crystal.";
        LOG(detail) << "    E_in: " << E_in;
        LOG(detail) << "    E_out: " << E_out;
        LOG(detail) << " => E_dep: " << E_in - E_out;
        continue;
      }
      LOG(detail) << "  \e[32mPassed.\e[0m Energy deposit is not negative.";
      //////////////////////////////////////////////////

      LOG(detail) << "  \e[01;32mAccepted.\e[0m";

      // FILL HISTOS
      // N_phe = E_digi * 1e3 * fNPhotoElectronsPerMeVAPDBarrel;
      nAccepted++;
      histoE_in->Fill(E_in);
      histoE_out->Fill(E_out);
      histoE_dep->Fill(E_in - E_out);
      histoE_digi->Fill(E_digi);
      histoR_LY->Fill(E_digi / (E_in - E_out));
      // histoR_phe->Fill(N_phe / (E_in - E_out));

      //////// Debug output ////////
      if (false)
      {
        std::cout << "\e[96m" << std::flush;
        std::cout << "Event:      " << nEvts << std::endl;
        std::cout << "\e[0m" << std::flush;

        std::cout << "localParticleCountEntering { ";
        for (auto elem : localParticleCountEntering)
          if (elem.second != 0)
            printf("%i => %i, ", elem.first, elem.second);
        std::cout << " }" << std::endl;

        std::cout << "localParticleCountExiting  { ";
        for (auto elem : localParticleCountExiting)
          if (elem.second != 0)
            printf("%i => %i, ", elem.first, elem.second);
        std::cout << " }" << std::endl;

        std::cout << "E_in:       " << E_in << std::endl;
        std::cout << "E_out:      " << E_out << std::endl;
        std::cout << "E_dep:      " << E_in - E_out << std::endl;
        std::cout << "R_LY:        " << E_digi / (E_in - E_out) << std::endl;
        std::cout << "\e[0m" << std::flush;
      }

      for (auto elem : localParticleCountEntering)
      {
        if (elem.second != 0)
        {
          histoParticlesEntering->Fill(std::to_string(elem.first).c_str(), elem.second, 1);
          globalParticleCountEntering[elem.first] += elem.second;
        }
      }
      for (auto elem : localParticleCountExiting)
      {
        if (elem.second != 0)
        {
          histoParticlesExiting->Fill(std::to_string(elem.first).c_str(), elem.second, 1);
          globalParticleCountExiting[elem.first] += elem.second;
        }
      }
    }
  }

  void Finish()
  {
    std::cout << "\e[96m" << std::flush;
    std::cout << "Analysis finished: " << nAccepted << " accepted of " << nTotal << " digis in " << nEvts << " events" << std::endl;

    std::cout << "globalParticleCountEntering { ";
    for (auto elem : globalParticleCountEntering)
      if (elem.second != 0)
        printf("%i => %i, ", elem.first, elem.second);
    std::cout << "}" << std::endl;
    std::cout << "globalParticleCountExiting { ";
    for (auto elem : globalParticleCountExiting)
      if (elem.second != 0)
        printf("%i => %i, ", elem.first, elem.second);
    std::cout << "}" << std::endl;
    std::cout << "\e[0m" << std::endl;

    std::cout << "\e[33m" << std::flush;
    std::cout << "eventFilterRejections { ";
    for (auto elem : eventFilterRejections)
      if (elem.second != 0)
        printf("%s => %i, ", elem.first.c_str(), elem.second);
    std::cout << "}" << std::endl;
    std::cout << "\e[0m" << std::flush;

    ofstream logfile("filterrejections.csv", std::ios::out | std::ios::app);
    logfile << now_YYYY_MM_DD_HH_mm_ss() << ",";
    logfile << fullname << ",";
    // logfile << name << ",";
    logfile << nEvts << ",";
    logfile << nTotal << ",";
    if (nTotal == 0)
      nTotal = 1;
    logfile << float(nAccepted) / nTotal * 100 << "%,";
    for (const auto &[key, value] : eventFilterRejections)
      logfile << float(value) / nTotal * 100 << "%,";
    logfile << std::endl;
    logfile.close();

    Double_t peakR_LY = histoR_LY->GetBinLowEdge(histoR_LY->GetMaximumBin());
    Double_t peakR_phe = histoR_phe->GetBinLowEdge(histoR_phe->GetMaximumBin());
    TFitResultPtr fitR_LY = histoR_LY->Fit("gaus", "SQ", "", peakR_LY - windowR_LY, peakR_LY + windowR_LY);
    TFitResultPtr fitR_phe = histoR_phe->Fit("gaus", "SQ", "", peakR_phe - windowR_phe, peakR_phe + windowR_phe);
    if (bool(int(fitR_LY))) // if fStatus not zero
      LOG(warn) << "Fit status not zero. Skipping `fitR_LY->Write()`";
    else
      fitR_LY->Write();
    if (bool(int(fitR_phe))) // if fStatus not zero
      LOG(warn) << "Fit status not zero. Skipping `fitR_phe->Write()`";
    else
      fitR_phe->Write();

    histoX_hit->Fill(temp->GetMean());
    histoX_hit->Write();
    histoE_digi->Write();
    histoR_phe->Write();
    histoE_in->Write();
    histoE_out->Write();
    histoE_dep->Write();
    histoR_LY->Write();
    histoN_hits->Write();
    histoParticlesEntering->Write();
    histoParticlesExiting->Write();
    histoEventFilter->Write();

    saveAs(histoX_hit, "plots/" + name + "_X_hit.png");
    saveAs(histoE_digi, "plots/" + name + "_E_digi.png");
    saveAs(histoR_phe, "plots/" + name + "_R_phe.png");
    saveAs(histoE_in, "plots/" + name + "_E_in.png");
    saveAs(histoE_out, "plots/" + name + "_E_out.png");
    saveAs(histoE_dep, "plots/" + name + "_E_dep.png");
    saveAs(histoR_LY, "plots/" + name + "_R_LY.png");
    saveAs(histoN_hits, "plots/" + name + "_N_hits.png");
    saveAs(histoParticlesEntering, "plots/" + name + "_N_ParticlesEntering.png");
    saveAs(histoParticlesExiting, "plots/" + name + "_N_ParticlesExiting.png");
    saveAs(histoEventFilter, "plots/" + name + "_EventFilter.png");
  }
};

void LY_ana(int nevts = 0, TString prefix = "dummy")
{
  auto logger = FairLogger::GetLogger();
  logger->SetLogToScreen(true);
  logger->SetLogScreenLevel("error");

  fullname = prefix;
  name = static_cast<std::string>(basename(prefix));

  PndMasterRunAna *fRun = new PndMasterRunAna();
  // fRun->SetInput("box");
  fRun->SetOutput("ana");
  fRun->AddFriend("pid");
  fRun->AddFriend("sim");
  fRun->SetParamRootFile(prefix + "_par.root");
  fRun->Setup(prefix);
  fRun->AddTask(new mytask());

  PndEmcMapper::Init(1);
  fRun->Init();
  fRun->Run(0, nevts);
  fRun->Finish();
}
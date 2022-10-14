#include <iostream>
#include <vector>
#include <algorithm>
#include <regex>
#include <string>
#include "dirent.h"

std::string dirname(std::string path)
{
  return path.substr(0, path.find((std::string)basename(path.c_str())));
}

std::string basename(std::string path)
{
  return (std::string)basename(path.c_str());
}

void GetFileList(std::string path, std::vector<std::string> &list)
{
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(path.c_str())) != NULL)
  {
    /* print all the files and directories within directory */
    while ((ent = readdir(dir)) != NULL)
    {
      list.push_back(ent->d_name);
    }
    closedir(dir);
    std::sort(list.begin(), list.end());
  }
  else
  {
    /* could not open directory */
    LOG(error) << "Could not open directory \"" << path << "\"";
    return;
  }
}

void FilterFileList(std::vector<std::string> &dirlist, std::string regex)
{
  std::regex pattern(regex);

  auto res = std::remove_if(dirlist.begin(), dirlist.end(), [pattern](std::string elem)
                            { return !std::regex_search(elem, pattern); });
  dirlist.erase(res, dirlist.end());
  LOG(info) << "Found " << dirlist.size() << " matches for \"" << regex << "\"";
}

void LY_extract(std::string path_and_prefix)
{
  // SETUP
  // gather files from regex
  std::string pwd = getenv("PWD");
  std::string inpath = dirname(path_and_prefix);
  std::string inprefix = basename(path_and_prefix);
  std::string path = pwd + "/" + inpath;
  LOG(info) << "Searching in " << path;

  std::vector<std::string> dirlist;
  GetFileList(path, dirlist);
  FilterFileList(dirlist, inprefix + std::string("-?[0-9]+_ana.root"));

  std::vector<std::tuple<Double_t, Double_t, Double_t, Double_t>> points;

  TCanvas *c = new TCanvas("LY", "Lightyield");

  // Prepare output file
  std::string filename = path + inprefix + std::string{"_LY.root"};
  TFile *datawriter = new TFile(TString{filename}, "RECREATE");
  datawriter->mkdir("histos");

  Double_t X0 = abs(56.952 - 76.952);
  Double_t X1 = abs(76.952 - 76.952);
  Double_t Y0 = 1e12;
  Double_t Y1 = -1;

  // User choices
  bool normalise = true;
  bool showRef = true;

  // Data collection from root files
  for (auto elem : dirlist)
  {
    LOG(info) << "Opening " << elem;
    TFile *datareader = new TFile(TString(path + elem), "READ");
    if (datareader == nullptr)
    {
      LOG(error) << "File not found: " << TString(path + elem);
      return;
    }

    /// do for all files
    TH1F *histoX_hit, *histoMode;
    TFitResult *res;
    datareader->GetObject("X_hit;1", histoX_hit);
    datareader->GetObject(TString(mode + ";1"), histoMode);
    double x, ex = 0;
    datareader->GetObject(TString("TFitResult-" + mode + "-gaus;1"), res);
    double y, ey = 0;
    if (histoX_hit == nullptr)
    {
      LOG(warn) << "Object not found: \"X_hit;1\"";
      x = 0;
      ex = 0;
      continue;
    }
    else
    {
      x = histoX_hit->GetMean();
      ex = 0.01;
    }
    if (res == nullptr)
    {
      LOG(warn) << "Object not found: \"TFitResult-" + mode + "-gaus;1\"";
      y = 0;
      ey = 0;
      continue;
    }
    else
    {
      y = res->Parameter(1);
      ey = res->Error(1);
    }
    LOG(detail) << " X:" << x << "\t Y:" << y;
    LOG(detail) << "eX:" << ex << "\teY:" << ey;
    points.push_back(std::make_tuple(x, y, ex, ey)); // store points to graph
    datawriter->cd("histos");
    histoMode->Write(TString::Format("X_%.1f", x)); // store histograms
  }

  TF2 *norm = new TF2("norm", "y/[0]");

  // Light yield filling and limits
  sort(points.begin(), points.end());
  TMultiGraph *mg = new TMultiGraph();
  TGraphErrors *LY = new TGraphErrors();
  for (auto &[x, y, ex, ey] : points)
  {
    x = abs(x - 76.952); // Shift and flip all points to align with APD/rear face at 0
    LY->SetPoint(LY->GetN(), x, y);
    LY->SetPointError(LY->GetN() - 1, ex, ey);
    if (y < Y0)
      Y0 = y;
    if (y > Y1)
      Y1 = y;
  }
  norm->SetParameter(0, Y0);
  if (normalise)
  {
    LY->Apply(norm);
    Y0 = norm->Eval(-1, Y0);
    Y1 = norm->Eval(-1, Y1);
  }

  // Disclaimer/Warning
  // I did never and probably will never understand how to properly draw a graph/histo/multigraph and write it to disk in CERN ROOT
  // I sincerely apologize about what follows...

  // Lightyield fitting and stat drawing (which doesn't draw...)
  LY->Fit("pol2", "WQ", "", 0, 20);
  TF1 *fitLY = LY->GetFunction("pol2");
  fitLY->SetLineColor(kRed);

  // REF
  datawriter->cd();

  TGraphErrors *REF1 = new TGraphErrors("Bremer_rLY_Type1_sim.csv", "%lg, %lg, %lg, %lg");
  TGraphErrors *REF2 = new TGraphErrors("Bremer_rLY_Type1_exp.csv", "%lg, %lg, %lg, %lg");
  TGraph *REF3 = new TGraph("p5Sim1.csv", "%lg,%*lg,%*lg,%*lg,%*lg,%*lg,%lg");
  TGraph *REF4 = new TGraph("p5Sim3.csv", "%lg,%*lg,%*lg,%*lg,%*lg,%*lg,%lg");
  REF1->SetTitle("Bremer, Simulation");
  REF2->SetTitle("Bremer, Experiment (Precision Setup)");
  REF3->SetTitle("p5.js, no absorption, no wrapping");
  REF4->SetTitle("p5.js, absorption, wrapping");
  REF1->Fit("pol2", "WQ", "", 0, 20);
  REF2->Fit("pol2", "WQ", "", 0, 20);
  REF3->Fit("pol2", "WQ", "", 0.9, 20);
  REF4->Fit("pol2", "WQ", "", 0.9, 20);
  TF1 *fitREF1 = REF1->GetFunction("pol2");
  TF1 *fitREF2 = REF2->GetFunction("pol2");
  TF1 *fitREF3 = REF3->GetFunction("pol2");
  TF1 *fitREF4 = REF4->GetFunction("pol2");
  fitREF1->SetLineColor(kGreen + 2);
  fitREF2->SetLineColor(kBlue - 4);
  fitREF3->SetLineColor(kYellow + 1);
  fitREF4->SetLineColor(kMagenta + 1);
  // REF1->SetMarkerColor(kGreen + 2);
  // REF2->SetMarkerColor(kBlue - 4);
  // REF3->SetMarkerColor(kYellow + 1);
  // REF4->SetMarkerColor(kMagenta + 1);
  REF1->SetMarkerStyle(5);
  REF2->SetMarkerStyle(5);
  REF3->SetMarkerStyle(kMultiply);
  REF4->SetMarkerStyle(kPlus);

  // Crystal edges marking
  TGraph *left = new TGraph(2, new const double[2]{X0, X0}, new const double[2]{1 * 0.98, 2 * 1.02}); // new TLine(X0, 0.9, X0, 1.2); // maybe?
  TGraph *right = new TGraph(2, new const double[2]{X1, X1}, new const double[2]{1 * 0.98, 2 * 1.02});
  left->SetLineWidth(4);
  left->SetLineColor(16);
  right->SetLineWidth(4);
  right->SetLineColor(16);

  // Conditional yAxisName
  std::string yAxisName = "normalized Light Yield (R_{LY}) [a.u.]";
  if (!normalise)
    yAxisName = "E_{digi} / E_{dep}";

  // Drawing
  TGraph *dummy_graph_to_extend_margins_beyond_actual_data = new TGraph(2, new const double[2]{-10, 30}, new const double[2]{1, 1});
  mg->Add(dummy_graph_to_extend_margins_beyond_actual_data, "AP");
  mg->Add(LY, "A*");
  mg->Add(left);
  mg->Add(right);
  if (showRef)
  {
    mg->Add(REF1, "AP");
    mg->Add(REF2, "AP");
    // mg->Add(REF3, "AP");
    // mg->Add(REF4, "AP");
  }
  // mg->SetNameTitle("LY", "Lightyield");
  mg->GetXaxis()->SetTitle("distance from APD (x) [cm]");
  mg->GetYaxis()->SetTitle(TString(yAxisName));
  mg->GetXaxis()->SetRangeUser(-1, 21);
  mg->GetYaxis()->SetRangeUser(0.95, 1.6);

  auto legend = new TLegend(0.15, 0.7, 0.8, 0.89);
  legend->AddEntry(fitLY, "PandaRoot (Muon, 80MeV)", "fitLY");
  legend->AddEntry(fitREF1, "Bremer\nSimulation (Simulation)", "fitREF1");
  legend->AddEntry(fitREF2, "Bremer\nExperiment (Precision Setup)", "fitREF2");
  // legend->AddEntry(fitREF3, "no absorption, no wrapping", "fitREF3");
  // legend->AddEntry(fitREF4, "w/ absorption, w/ wrapping", "fitREF4");

  mg->Draw("SAME");
  right->Draw("SAME");
  left->Draw("SAME");
  legend->Draw("SAME");
  // dummy_graph_to_extend_margins_beyond_actual_data->Draw("SAME");
  // mg->Write();
  c->SetRightMargin(0.01);
  c->SetLeftMargin(0.1);
  c->SetTopMargin(0.02);

  c->Write();

  LOG(info) << "Graph written to file \"" << filename << "\" with " << points.size() << " points";
}

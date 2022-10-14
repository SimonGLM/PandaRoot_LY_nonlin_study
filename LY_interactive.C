/// \file
/// \ingroup tutorial_graPads
///
/// This tutorial demonstrates how to use the highlight mode on graph.
///
/// \macro_code
///
/// \date March 2018
/// \author Jan Musinsky

TList *l = 0;

void HighlightHisto(TVirtualPad *pad, TObject *obj, Int_t ihp, Int_t y);

void LY_interactive(TString filename)
{
  auto Canvas = new TCanvas("Canvas", "Canvas", 0, 0, 700, 500);

  TH1F *h;
  TFile *f = new TFile(filename, "READ");
  TDirectoryFile *dir;
  f->GetObject("histos", dir);
  l = new TList();

  Double_t max = 0.0;
  for (auto keyAsObj : *dir->GetListOfKeys()) {
    auto key = dynamic_cast<TKey *>(keyAsObj);
    TObject *o = dir->Get(key->GetName());
    h = (TH1F *)o;
    max = (h->GetMaximum() > max ? h->GetMaximum() : max);
    l->Add(h);
  }
  for (auto h : *l)
    ((TH1F *)h)->SetMaximum(max);

  f->cd();
  TGraphErrors *g;
  f->GetObject("LY;1", g);
  // g->SetMarkerStyle(6);
  g->Draw("AP*");

  auto Pad = new TPad("histopad", "Pad for source histo", 0.4, 0.5, 0.9, 0.9);
  Pad->SetFillStyle(4004);
  Pad->Draw();
  Pad->cd();
  auto info = new TText(0.5, 0.5, "please move the mouse over the graPad");
  info->SetTextAlign(22);
  info->Draw();
  Canvas->cd();

  g->SetHighlight();
  Canvas->HighlightConnect("HighlightHisto(TVirtualPad*,TObject*,Int_t,Int_t)");
}

void HighlightHisto(TVirtualPad *pad, TObject *obj, Int_t ihp, Int_t y)
{
  auto Pad = (TVirtualPad *)pad->FindObject("histopad");
  if (!Pad) {
    return;
  }

  if (ihp == -1) { // after highlight disabled
    Pad->Clear();
    return;
  }

  Pad->cd();
  l->At(ihp)->Draw();
  gPad->Update();
}

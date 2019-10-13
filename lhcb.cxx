/**
 * Copyright CERN; jblomer@cern.ch
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include <ROOT/RDataFrame.hxx>
#include <ROOT/RNTuple.hxx>
#include <ROOT/RNTupleDS.hxx>
#include <ROOT/RNTupleOptions.hxx>
#include <Compression.h>
#include <TApplication.h>
#include <TBranch.h>
#include <TCanvas.h>
#include <TClassTable.h>
#include <TFile.h>
#include <TH1D.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreePerfStats.h>

#include "util.h"

bool g_perf_stats = false;
bool g_show = false;
bool g_use_imt = false;
unsigned int g_nstreams = 0;
bool g_mmap = false;

static ROOT::Experimental::RNTupleReadOptions GetRNTupleOptions() {
   using RNTupleReadOptions = ROOT::Experimental::RNTupleReadOptions;

   RNTupleReadOptions options;
   if (g_mmap) {
      options.SetClusterCache(RNTupleReadOptions::kMMap);
      std::cout << "{Using MMAP cluster pool}" << std::endl;
   } else {
      options.SetClusterCache(RNTupleReadOptions::kOn);
      std::cout << "{Using async cluster pool}" << std::endl;
      if (g_nstreams > 0)
         options.SetNumStreams(g_nstreams);
      std::cout << "{Using " << options.GetNumStreams() << " streams}" << std::endl;
   }
   return options;
}

constexpr double kKaonMassMeV = 493.677;


static void Show(TH1D *h) {
   new TApplication("", nullptr, nullptr);

   gStyle->SetTextFont(42);
   auto c = new TCanvas("c", "", 800, 700);
   h->GetXaxis()->SetTitle("m_{KKK} [MeV/c^{2}]");
   h->DrawCopy();
   c->Modified();

   std::cout << "press ENTER to exit..." << std::endl;
   auto future = std::async(std::launch::async, getchar);
   while (true) {
      gSystem->ProcessEvents();
      if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
         break;
   }
}


static double GetP2(unsigned int /*slot*/, double px, double py, double pz)
{
   return px*px + py*py + pz*pz;
}

static double GetKE(unsigned int /*slot*/, double px, double py, double pz)
{
   double p2 = GetP2(0, px, py, pz);
   return sqrt(p2 + kKaonMassMeV*kKaonMassMeV);
}



static void Dataframe(ROOT::RDataFrame &frame, int nslots)
{
   auto ts_init = std::chrono::steady_clock::now();
   std::chrono::steady_clock::time_point ts_first;
   bool ts_first_set = false;

   auto fn_muon_cut = [](int is_muon) { return !is_muon; };
   auto fn_k_cut = [](double prob_k) { return prob_k > 0.5; };
   auto fn_pi_cut = [](double prob_pi) { return prob_pi < 0.5; };
   auto fn_sum = [](unsigned int slot, double p1, double p2, double p3) { return p1 + p2 + p3; };
   auto fn_mass = [](unsigned int slot, double B_E, double B_P2) { double r = sqrt(B_E*B_E - B_P2); return r; };

   auto df_timing = frame.DefineSlot("TIMING", [&ts_first, &ts_first_set](unsigned int slot) {
      if (slot > 0)
         return true;
      if (!ts_first_set)
         ts_first = std::chrono::steady_clock::now();
      ts_first_set = true;
      return ts_first_set;}).Filter([](bool b){ return b; }, {"TIMING"});

   auto df_muon_cut = df_timing.Filter(fn_muon_cut, {"H1_isMuon"})
                               .Filter(fn_muon_cut, {"H2_isMuon"})
                               .Filter(fn_muon_cut, {"H3_isMuon"});
   auto df_k_cut = df_muon_cut.Filter(fn_k_cut, {"H1_ProbK"})
                              .Filter(fn_k_cut, {"H2_ProbK"})
                              .Filter(fn_k_cut, {"H3_ProbK"});
   auto df_pi_cut = df_k_cut.Filter(fn_pi_cut, {"H1_ProbPi"})
                            .Filter(fn_pi_cut, {"H2_ProbPi"})
                            .Filter(fn_pi_cut, {"H3_ProbPi"});
   auto df_mass = df_pi_cut.DefineSlot("B_PX", fn_sum, {"H1_PX", "H2_PX", "H3_PX"})
                           .DefineSlot("B_PY", fn_sum, {"H1_PY", "H2_PY", "H3_PY"})
                           .DefineSlot("B_PZ", fn_sum, {"H1_PZ", "H2_PZ", "H3_PZ"})
                           .DefineSlot("B_P2", GetP2, {"B_PX", "B_PY", "B_PZ"})
                           .DefineSlot("K1_E", GetKE, {"H1_PX", "H1_PY", "H1_PZ"})
                           .DefineSlot("K2_E", GetKE, {"H2_PX", "H2_PY", "H2_PZ"})
                           .DefineSlot("K3_E", GetKE, {"H3_PX", "H3_PY", "H3_PZ"})
                           .DefineSlot("B_E", fn_sum, {"K1_E", "K2_E", "K3_E"})
                           .DefineSlot("B_m", fn_mass, {"B_E", "B_P2"});
   auto hMass = df_mass.Histo1D({"B_mass", "", 500, 5050, 5500}, "B_m");

   *hMass;
   auto ts_end = std::chrono::steady_clock::now();
   auto runtime_init = std::chrono::duration_cast<std::chrono::microseconds>(ts_first - ts_init).count();
   auto runtime_analyze = std::chrono::duration_cast<std::chrono::microseconds>(ts_end - ts_first).count();
   std::cout << "Runtime-Initialization: " << runtime_init << "us" << std::endl;
   std::cout << "Runtime-Analysis: " << runtime_analyze << "us" << std::endl;

   if (g_show)
      Show(hMass.GetPtr());
}


static void TreeDirect(const std::string &path) {
   auto ts_init = std::chrono::steady_clock::now();

   auto file = TFile::Open(path.c_str());
   auto tree = file->Get<TTree>("DecayTree");
   TTreePerfStats *ps = nullptr;
   if (g_perf_stats)
      ps = new TTreePerfStats("ioperf", tree);

   TBranch *br_h1_px = nullptr;
   TBranch *br_h1_py = nullptr;
   TBranch *br_h1_pz = nullptr;
   TBranch *br_h1_prob_k = nullptr;
   TBranch *br_h1_prob_pi = nullptr;
   TBranch *br_h1_is_muon = nullptr;
   TBranch *br_h2_px = nullptr;
   TBranch *br_h2_py = nullptr;
   TBranch *br_h2_pz = nullptr;
   TBranch *br_h2_prob_k = nullptr;
   TBranch *br_h2_prob_pi = nullptr;
   TBranch *br_h2_is_muon = nullptr;
   TBranch *br_h3_px = nullptr;
   TBranch *br_h3_py = nullptr;
   TBranch *br_h3_pz = nullptr;
   TBranch *br_h3_prob_k = nullptr;
   TBranch *br_h3_prob_pi = nullptr;
   TBranch *br_h3_is_muon = nullptr;

   double h1_px;
   double h1_py;
   double h1_pz;
   double h1_prob_k;
   double h1_prob_pi;
   int h1_is_muon;
   double h2_px;
   double h2_py;
   double h2_pz;
   double h2_prob_k;
   double h2_prob_pi;
   int h2_is_muon;
   double h3_px;
   double h3_py;
   double h3_pz;
   double h3_prob_k;
   double h3_prob_pi;
   int h3_is_muon;

   tree->SetBranchAddress("H1_PX",     &h1_px,      &br_h1_px);
   tree->SetBranchAddress("H1_PY",     &h1_py,      &br_h1_py);
   tree->SetBranchAddress("H1_PZ",     &h1_pz,      &br_h1_pz);
   tree->SetBranchAddress("H1_ProbK",  &h1_prob_k,  &br_h1_prob_k);
   tree->SetBranchAddress("H1_ProbPi", &h1_prob_pi, &br_h1_prob_pi);
   tree->SetBranchAddress("H1_isMuon", &h1_is_muon, &br_h1_is_muon);
   tree->SetBranchAddress("H2_PX",     &h2_px,      &br_h2_px);
   tree->SetBranchAddress("H2_PY",     &h2_py,      &br_h2_py);
   tree->SetBranchAddress("H2_PZ",     &h2_pz,      &br_h2_pz);
   tree->SetBranchAddress("H2_ProbK",  &h2_prob_k,  &br_h2_prob_k);
   tree->SetBranchAddress("H2_ProbPi", &h2_prob_pi, &br_h2_prob_pi);
   tree->SetBranchAddress("H2_isMuon", &h2_is_muon, &br_h2_is_muon);
   tree->SetBranchAddress("H3_PX",     &h3_px,      &br_h3_px);
   tree->SetBranchAddress("H3_PY",     &h3_py,      &br_h3_py);
   tree->SetBranchAddress("H3_PZ",     &h3_pz,      &br_h3_pz);
   tree->SetBranchAddress("H3_ProbK",  &h3_prob_k,  &br_h3_prob_k);
   tree->SetBranchAddress("H3_ProbPi", &h3_prob_pi, &br_h3_prob_pi);
   tree->SetBranchAddress("H3_isMuon", &h3_is_muon, &br_h3_is_muon);

   auto hMass = new TH1D("B_mass", "", 500, 5050, 5500);

   auto nEntries = tree->GetEntries();
   std::chrono::steady_clock::time_point ts_first;
   for (decltype(nEntries) entryId = 0; entryId < nEntries; ++entryId) {
      if ((entryId % 100000) == 0) {
         printf("processed %llu k events\n", entryId / 1000);
         //printf("dummy is %lf\n", dummy); abort();
      }
      if (entryId == 1) {
         ts_first = std::chrono::steady_clock::now();
      }

      br_h1_is_muon->GetEntry(entryId);
      if (h1_is_muon) continue;
      br_h2_is_muon->GetEntry(entryId);
      if (h2_is_muon) continue;
      br_h3_is_muon->GetEntry(entryId);
      if (h3_is_muon) continue;

      constexpr double prob_k_cut = 0.5;
      br_h1_prob_k->GetEntry(entryId);
      if (h1_prob_k < prob_k_cut) continue;
      br_h2_prob_k->GetEntry(entryId);
      if (h2_prob_k < prob_k_cut) continue;
      br_h3_prob_k->GetEntry(entryId);
      if (h3_prob_k < prob_k_cut) continue;

      constexpr double prob_pi_cut = 0.5;
      br_h1_prob_pi->GetEntry(entryId);
      if (h1_prob_pi > prob_pi_cut) continue;
      br_h2_prob_pi->GetEntry(entryId);
      if (h2_prob_pi > prob_pi_cut) continue;
      br_h3_prob_pi->GetEntry(entryId);
      if (h3_prob_pi > prob_pi_cut) continue;

      br_h1_px->GetEntry(entryId);
      br_h1_py->GetEntry(entryId);
      br_h1_pz->GetEntry(entryId);
      br_h2_px->GetEntry(entryId);
      br_h2_py->GetEntry(entryId);
      br_h2_pz->GetEntry(entryId);
      br_h3_px->GetEntry(entryId);
      br_h3_py->GetEntry(entryId);
      br_h3_pz->GetEntry(entryId);

      double b_px = h1_px + h2_px + h3_px;
      double b_py = h1_py + h2_py + h3_py;
      double b_pz = h1_pz + h2_pz + h3_pz;
      double b_p2 = GetP2(0, b_px, b_py, b_pz);
      double k1_E = GetKE(0, h1_px, h1_py, h1_pz);
      double k2_E = GetKE(0, h2_px, h2_py, h2_pz);
      double k3_E = GetKE(0, h3_px, h3_py, h3_pz);
      double b_E = k1_E + k2_E + k3_E;
      double b_mass = sqrt(b_E*b_E - b_p2);
      hMass->Fill(b_mass);

      //printf("BMASS %lf\n", b_mass);
   }

   auto ts_end = std::chrono::steady_clock::now();
   auto runtime_init = std::chrono::duration_cast<std::chrono::microseconds>(ts_first - ts_init).count();
   auto runtime_analyze = std::chrono::duration_cast<std::chrono::microseconds>(ts_end - ts_first).count();

   std::cout << "Runtime-Initialization: " << runtime_init << "us" << std::endl;
   std::cout << "Runtime-Analysis: " << runtime_analyze << "us" << std::endl;

   if (g_perf_stats)
      ps->Print();
   if (g_show) {
      Show(hMass);
   }

   delete hMass;
}


static void NTupleDirect(const std::string &path)
{
   using RNTupleReader = ROOT::Experimental::RNTupleReader;

   auto ts_init = std::chrono::steady_clock::now();

   auto options = GetRNTupleOptions();
   auto ntuple = RNTupleReader::Open("DecayTree", path, options);
   if (g_perf_stats)
      ntuple->EnableMetrics();

   auto viewH1IsMuon = ntuple->GetView<int>("H1_isMuon");
   auto viewH2IsMuon = ntuple->GetView<int>("H2_isMuon");
   auto viewH3IsMuon = ntuple->GetView<int>("H3_isMuon");

   auto viewH1PX = ntuple->GetView<double>("H1_PX");
   auto viewH1PY = ntuple->GetView<double>("H1_PY");
   auto viewH1PZ = ntuple->GetView<double>("H1_PZ");
   auto viewH1ProbK = ntuple->GetView<double>("H1_ProbK");
   auto viewH1ProbPi = ntuple->GetView<double>("H1_ProbPi");

   auto viewH2PX = ntuple->GetView<double>("H2_PX");
   auto viewH2PY = ntuple->GetView<double>("H2_PY");
   auto viewH2PZ = ntuple->GetView<double>("H2_PZ");
   auto viewH2ProbK = ntuple->GetView<double>("H2_ProbK");
   auto viewH2ProbPi = ntuple->GetView<double>("H2_ProbPi");

   auto viewH3PX = ntuple->GetView<double>("H3_PX");
   auto viewH3PY = ntuple->GetView<double>("H3_PY");
   auto viewH3PZ = ntuple->GetView<double>("H3_PZ");
   auto viewH3ProbK = ntuple->GetView<double>("H3_ProbK");
   auto viewH3ProbPi = ntuple->GetView<double>("H3_ProbPi");

   auto hMass = new TH1D("B_mass", "", 500, 5050, 5500);

   unsigned nevents = 0;
   std::chrono::steady_clock::time_point ts_first;
   for (auto i : ntuple->GetViewRange()) {
      nevents++;
      if ((nevents % 100000) == 0) {
         printf("processed %u k events\n", nevents / 1000);
         //printf("dummy is %lf\n", dummy); abort();
      }
      if (nevents == 1) {
         ts_first = std::chrono::steady_clock::now();
      }

      if (viewH1IsMuon(i) || viewH2IsMuon(i) || viewH3IsMuon(i)) {
         continue;
      }

      constexpr double prob_k_cut = 0.5;
      if (viewH1ProbK(i) < prob_k_cut) continue;
      if (viewH2ProbK(i) < prob_k_cut) continue;
      if (viewH3ProbK(i) < prob_k_cut) continue;

      constexpr double prob_pi_cut = 0.5;
      if (viewH1ProbPi(i) > prob_pi_cut) continue;
      if (viewH2ProbPi(i) > prob_pi_cut) continue;
      if (viewH3ProbPi(i) > prob_pi_cut) continue;

      double b_px = viewH1PX(i) + viewH2PX(i) + viewH3PX(i);
      double b_py = viewH1PY(i) + viewH2PY(i) + viewH3PY(i);
      double b_pz = viewH1PZ(i) + viewH2PZ(i) + viewH3PZ(i);
      double b_p2 = GetP2(0, b_px, b_py, b_pz);
      double k1_E = GetKE(0, viewH1PX(i), viewH1PY(i), viewH1PZ(i));
      double k2_E = GetKE(0, viewH2PX(i), viewH2PY(i), viewH2PZ(i));
      double k3_E = GetKE(0, viewH3PX(i), viewH3PY(i), viewH3PZ(i));
      double b_E = k1_E + k2_E + k3_E;
      double b_mass = sqrt(b_E*b_E - b_p2);
      hMass->Fill(b_mass);
   }
   auto ts_end = std::chrono::steady_clock::now();
   auto runtime_init = std::chrono::duration_cast<std::chrono::microseconds>(ts_first - ts_init).count();
   auto runtime_analyze = std::chrono::duration_cast<std::chrono::microseconds>(ts_end - ts_first).count();

   std::cout << "Runtime-Initialization: " << runtime_init << "us" << std::endl;
   std::cout << "Runtime-Analysis: " << runtime_analyze << "us" << std::endl;

   if (g_perf_stats)
      ntuple->PrintInfo(ROOT::Experimental::ENTupleInfo::kMetrics);
   if (g_show)
      Show(hMass);

   delete hMass;
}


static void Usage(const char *progname) {
  printf("%s [-i input.root] [-r(df) / -R(df / MT)] [-p(erformance stats)] [-s(show)]\n"
         "   [-c #streams] [-m(map)]\n", progname);
}


int main(int argc, char **argv) {
   std::string input_path;
   std::string input_suffix;
   bool use_rdf = false;
   int c;
   while ((c = getopt(argc, argv, "hvi:rRpsc:m")) != -1) {
      switch (c) {
      case 'h':
      case 'v':
         Usage(argv[0]);
         return 0;
      case 'i':
         input_path = optarg;
         break;
      case 'p':
         g_perf_stats = true;
         break;
      case 's':
         g_show = true;
         break;
      case 'r':
         use_rdf = true;
         break;
      case 'R':
         use_rdf = true;
         g_use_imt = true;
         break;
      case 'c':
         g_nstreams = std::stoi(optarg);
         break;
      case 'm':
         g_mmap = true;
         break;
      default:
         fprintf(stderr, "Unknown option: -%c\n", c);
         Usage(argv[0]);
         return 1;
      }
   }
   if (input_path.empty()) {
      Usage(argv[0]);
      return 1;
   }

   if (g_use_imt) {
      ROOT::EnableImplicitMT();
      std::cout << "Running multi-threaded with " << ROOT::GetImplicitMTPoolSize() << " slots" << std::endl;
   }

   auto suffix = GetSuffix(input_path);
   switch (GetFileFormat(suffix)) {
   case FileFormats::kRoot:
      if (use_rdf) {
         ROOT::RDataFrame df("DecayTree", input_path);
         Dataframe(df, 1);
      } else {
         TreeDirect(input_path);
      }
      break;
   case FileFormats::kNtuple:
      if (use_rdf) {
         using RNTupleDS = ROOT::Experimental::RNTupleDS;
         auto options = GetRNTupleOptions();
         auto pageSource = ROOT::Experimental::Detail::RPageSource::Create("DecayTree", input_path, options);
         ROOT::RDataFrame df(std::make_unique<RNTupleDS>(std::move(pageSource)));
         Dataframe(df, 1);
      } else {
         NTupleDirect(input_path);
      }
      break;
   default:
      std::cerr << "Invalid file format: " << suffix << std::endl;
      return 1;
   }

   return 0;
}

// Merge ONLY the 13 jet-charge QA histograms from per-group ROOT files.
// Validates each group's plotting cuts, reconstruction labels and binning.
// This avoids ROOT hadd's ambiguous behavior for non-histogram metadata.
// Example (from analysis/):
// root -l -b -q 'merge_jetcharge_histograms.C("plots_run79516/groups","plots_run79516/run79516_histograms.root",38)'

#include <TFile.h>
#include <TH1.h>
#include <TNamed.h>
#include <TParameter.h>
#include <TSystem.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <vector>

int merge_jetcharge_histograms(const char* groupsDir,
                                const char* outputFile,
                                int expectedGroups = 38)
{
  if (!groupsDir || !*groupsDir || !outputFile || !*outputFile)
  {
    std::cerr << "ERROR: missing group directory or output file.\n";
    return 1;
  }

  void* dir = gSystem->OpenDirectory(groupsDir);
  if (!dir)
  {
    std::cerr << "ERROR: cannot open group directory: " << groupsDir << '\n';
    return 1;
  }
  std::vector<std::string> files;
  const std::regex groupName(R"(group_[0-9][0-9])");
  const char* entry = nullptr;
  while ((entry = gSystem->GetDirEntry(dir)))
  {
    if (!std::regex_match(entry, groupName)) continue;
    std::string path = std::string(groupsDir) + "/" + entry +
                       "/jetcharge_histograms.root";
    if (gSystem->AccessPathName(path.c_str()))
    {
      std::cerr << "ERROR: missing group histogram: " << path << '\n';
      gSystem->FreeDirectory(dir);
      return 1;
    }
    files.push_back(path);
  }
  gSystem->FreeDirectory(dir);
  std::sort(files.begin(), files.end());
  if (files.empty() || (expectedGroups > 0 &&
      static_cast<int>(files.size()) != expectedGroups))
  {
    std::cerr << "ERROR: found " << files.size() << " groups, expected "
              << expectedGroups << '\n';
    return 1;
  }

  const std::vector<std::string> names = {
    "hPtAll", "hPtFid", "hJetsPerEvent", "hEta", "hPhi", "hMass",
    "hChargeAll", "hChargeMatched", "hCharge2to5", "hCharge5to10",
    "hCharge10plus", "hNMatched", "hChargeVsPt"};
  TH1::AddDirectory(kFALSE);
  std::map<std::string, std::unique_ptr<TH1>> merged;
  std::string mode, jetTag, towerTag;
  double minPt = 0., maxEta = 0.;
  Long64_t totalEvents = 0;
  bool first = true;

  for (const auto& path : files)
  {
    TFile input(path.c_str(), "READ");
    if (input.IsZombie())
    {
      std::cerr << "ERROR: cannot read group file " << path << '\n';
      return 1;
    }

    auto* recMode = dynamic_cast<TNamed*>(input.Get("jetcharge_reco_mode"));
    auto* recJet = dynamic_cast<TNamed*>(input.Get("jetcharge_jet_cut_tag"));
    auto* recTower = dynamic_cast<TNamed*>(input.Get("jetcharge_tower_cut_tag"));
    auto* pt = dynamic_cast<TParameter<double>*>(input.Get("jetcharge_plot_min_pt"));
    auto* eta = dynamic_cast<TParameter<double>*>(input.Get("jetcharge_plot_max_abs_eta"));
    auto* events = dynamic_cast<TParameter<Long64_t>*>(input.Get("jetcharge_events_read"));
    if (!recMode || !recJet || !recTower || !pt || !eta || !events ||
        !std::isfinite(pt->GetVal()) || !std::isfinite(eta->GetVal()) ||
        events->GetVal() <= 0)
    {
      std::cerr << "ERROR: missing or bad group metadata in " << path << '\n';
      return 1;
    }
    if (first)
    {
      mode = recMode->GetTitle();
      jetTag = recJet->GetTitle();
      towerTag = recTower->GetTitle();
      minPt = pt->GetVal();
      maxEta = eta->GetVal();
    }
    else if (mode != recMode->GetTitle() ||
             jetTag != recJet->GetTitle() ||
             towerTag != recTower->GetTitle() ||
             std::abs(minPt - pt->GetVal()) > 1e-9 ||
             std::abs(maxEta - eta->GetVal()) > 1e-9)
    {
      std::cerr << "ERROR: reconstruction labels or plot cuts differ: "
                << path << '\n';
      return 1;
    }
    totalEvents += events->GetVal();

    for (const auto& name : names)
    {
      auto* hist = dynamic_cast<TH1*>(input.Get(name.c_str()));
      if (!hist)
      {
        std::cerr << "ERROR: missing histogram " << name << " in " << path << '\n';
        return 1;
      }
      auto it = merged.find(name);
      if (it == merged.end())
      {
        std::unique_ptr<TH1> clone(dynamic_cast<TH1*>(hist->Clone(name.c_str())));
        if (!clone)
        {
          std::cerr << "ERROR: cannot clone histogram " << name << '\n';
          return 1;
        }
        clone->SetDirectory(nullptr);
        merged.emplace(name, std::move(clone));
      }
      else
      {
        TH1* dst = it->second.get();
        if (dst->IsA() != hist->IsA() ||
            dst->GetNbinsX() != hist->GetNbinsX() ||
            dst->GetNbinsY() != hist->GetNbinsY() ||
            dst->GetNbinsZ() != hist->GetNbinsZ() ||
            dst->GetXaxis()->GetXmin() != hist->GetXaxis()->GetXmin() ||
            dst->GetXaxis()->GetXmax() != hist->GetXaxis()->GetXmax() ||
            dst->GetYaxis()->GetXmin() != hist->GetYaxis()->GetXmin() ||
            dst->GetYaxis()->GetXmax() != hist->GetYaxis()->GetXmax() ||
            !dst->Add(hist))
        {
          std::cerr << "ERROR: incompatible histogram " << name << " in "
                    << path << '\n';
          return 1;
        }
      }
    }
    first = false;
  }

  const std::string output(outputFile);
  const auto slash = output.find_last_of('/');
  if (slash != std::string::npos && slash > 0)
  {
    const auto dirName = output.substr(0, slash);
    if (gSystem->mkdir(dirName.c_str(), true) != 0 &&
        gSystem->AccessPathName(dirName.c_str()))
      return 1;
  }
  TFile file(outputFile, "RECREATE");
  if (file.IsZombie())
  {
    std::cerr << "ERROR: cannot create merged file " << outputFile << '\n';
    return 1;
  }
  for (const auto& name : names) merged.at(name)->Write();
  TNamed("jetcharge_reco_mode", mode.c_str()).Write();
  TNamed("jetcharge_jet_cut_tag", jetTag.c_str()).Write();
  TNamed("jetcharge_tower_cut_tag", towerTag.c_str()).Write();
  TParameter<double>("jetcharge_plot_min_pt", minPt).Write();
  TParameter<double>("jetcharge_plot_max_abs_eta", maxEta).Write();
  TParameter<Long64_t>("jetcharge_events_read", totalEvents).Write();
  file.Close();
  std::cout << "Merged " << files.size() << " groups, " << totalEvents
            << " event entries into " << outputFile << '\n';
  return 0;
}

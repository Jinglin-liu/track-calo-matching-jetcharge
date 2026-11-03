#ifndef JET_CHARGE_PLOT_HELPERS_H
#define JET_CHARGE_PLOT_HELPERS_H

// Common reconstruction-mode labels for the jet-charge plotting macros.
// Filename/directory tags are annotations, not verified reconstruction metadata.
#include <algorithm>
#include <cctype>
#include <iostream>
#include <regex>
#include <string>

namespace JetChargePlots
{
  struct JetPlotSource
  {
    std::string mode = "Calorimeter";
    std::string jetCut;
    std::string towerCut;
  };

  inline JetPlotSource parseJetPlotSource(const std::string& path)
  {
    const auto lower = [](std::string value) {
      std::transform(value.begin(), value.end(), value.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      return value;
    };
    const auto slash = path.find_last_of("/\\");
    const std::string basename = lower(path.substr(slash == std::string::npos ? 0 : slash + 1));
    const std::string directory = lower(slash == std::string::npos ? "" : path.substr(0, slash));

    const auto modeOf = [](const std::string& value) {
      if (value.find("sub1") != std::string::npos) return 1;
      if (value.find("unsubtracted") != std::string::npos ||
          value.find("unsub") != std::string::npos) return 2;
      return 0;
    };
    const int fileMode = modeOf(basename);
    const int dirMode = modeOf(directory);
    if (fileMode && dirMode && fileMode != dirMode)
      std::cerr << "WARNING: jet mode in filename conflicts with directory; "
                   "using filename mode.\n";

    JetPlotSource result;
    const int mode = fileMode ? fileMode : dirMode;
    if (mode == 1) result.mode = "Sub1";
    else if (mode == 2) result.mode = "Unsubtracted";
    else std::cerr << "WARNING: cannot determine jet mode from input path; "
                      "using generic calorimeter label.\n";

    // Directory names are user-provided metadata, NOT verified TTree cuts.
    // Prefer tags in the parent directory to an occasional tagged file name.
    const std::regex jetRe(R"((?:^|[/_])([0-9]+(?:\.[0-9]+)?)gev(?:[/_]|$))");
    const std::regex towerRe(R"((?:^|[/_])([0-9]+(?:\.[0-9]+)?)tower(?:[/_]|$))");
    std::smatch match;
    if (std::regex_search(directory, match, jetRe) ||
        std::regex_search(basename, match, jetRe))
      result.jetCut = match[1].str();
    if (std::regex_search(directory, match, towerRe) ||
        std::regex_search(basename, match, towerRe))
      result.towerCut = match[1].str();
    return result;
  }
}


#endif  // JET_CHARGE_PLOT_HELPERS_H

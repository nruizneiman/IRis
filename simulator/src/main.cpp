// Usage: iris_sim                interactive prompt
//        iris_sim <script.txt>   runs a script, echoing every command ('#' lines are comments)
#include <fstream>
#include <iostream>

#include "Simulator.h"

int main(int argc, char** argv) {
  iris::sim::Simulator sim;
  std::istream* in = &std::cin;
  std::ifstream file;
  const bool scripted = argc > 1;
  if (scripted) {
    file.open(argv[1]);
    if (!file) {
      std::cerr << "cannot open script: " << argv[1] << "\n";
      return 2;
    }
    in = &file;
  } else {
    std::cout << "IRis console simulator (all hardware simulated). Type 'help'.\n";
  }
  std::string line;
  while (true) {
    if (!scripted) std::cout << "iris> " << std::flush;
    if (!std::getline(*in, line)) break;
    if (line == "quit" || line == "exit") break;
    if (scripted) {
      if (line.empty()) continue;
      std::cout << (line[0] == '#' ? "" : "iris> ") << line << "\n";
      if (line[0] == '#') continue;
    }
    const std::string out = sim.execute(line);
    if (!out.empty()) std::cout << out << "\n";
  }
  return 0;
}

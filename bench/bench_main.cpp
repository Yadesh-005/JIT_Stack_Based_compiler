// bench_main.cpp -- benchmark harness: interpreter vs. JIT configurations.
//
// For every bench/programs/*.jbc it runs each configuration REPS times,
// takes the median wall-clock time of VirtualMachine::run() (assembly and
// verification excluded), checks that every configuration produced exactly
// the interpreter's output, and writes results/bench_<arch>.csv plus a
// Markdown table.
//
// usage: jit_bench [--dir bench/programs] [--reps 5] [--out results] [--only NAME]
#include <algorithm>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "assembler.h"
#include "backend.h"
#include "vm.h"

namespace {
struct Config {
    std::string name;
    bool jit;
    VMConfig::Backend backend;
    int opt;
};

struct Result {
    double median = 0, best = 0;
    std::string output;
    int traces = 0;
    int loopOps = -1;
    double jitShare = 0;
};

Result measure(const Program& p, const Config& c, int reps) {
    Result r;
    std::vector<double> times;
    for (int i = 0; i < reps; i++) {
        VMConfig cfg;
        cfg.jit = c.jit;
        cfg.backend = c.backend;
        cfg.optLevel = c.opt;
        cfg.verbosity = 0;
        VirtualMachine vm(cfg);
        std::ostringstream out;
        vm.setOutput(&out);
        vm.setLog(nullptr);
        vm.loadProgram(p);
        auto t0 = std::chrono::steady_clock::now();
        vm.run();
        auto t1 = std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double>(t1 - t0).count());
        r.output = out.str();
        r.traces = vm.stats().tracesCompiled;
        if (!vm.traces().empty()) r.loopOps = vm.traces()[0]->ssa.stats.loopOps;
        double total = (double)vm.stats().interpreted + (double)vm.stats().traceIterations;
        r.jitShare = total > 0 ? 100.0 * (double)vm.stats().traceIterations / total : 0;
    }
    std::sort(times.begin(), times.end());
    r.median = times[times.size() / 2];
    r.best = times.front();
    return r;
}
}  // namespace

int main(int argc, char** argv) {
    std::string dir = "bench/programs", outDir = "results", only;
    int reps = 5;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--dir" && i + 1 < argc) dir = argv[++i];
        else if (a == "--reps" && i + 1 < argc) reps = std::max(1, std::stoi(argv[++i]));
        else if (a == "--out" && i + 1 < argc) outDir = argv[++i];
        else if (a == "--only" && i + 1 < argc) only = argv[++i];
    }
    const bool native = nativeBackendAvailable();
    std::vector<Config> configs = {
        {"Interpreter", false, VMConfig::Backend::IR, 2},
        {"JIT IR-executor -O2", true, VMConfig::Backend::IR, 2},
        {"JIT native -O0", true, VMConfig::Backend::Native, 0},
        {"JIT native -O1", true, VMConfig::Backend::Native, 1},
        {"JIT native -O2", true, VMConfig::Backend::Native, 2},
    };
    std::vector<std::string> files;
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() > 4 && n.substr(n.size() - 4) == ".jbc" && (only.empty() || n.find(only) != std::string::npos))
                files.push_back(n);
        }
        closedir(d);
    }
    std::sort(files.begin(), files.end());
    std::string arch = nativeArchName();
    std::cout << "Benchmark: " << files.size() << " programs x " << configs.size() << " configurations, median of "
              << reps << " runs\n";
    std::cout << "host: " << arch << ", compiler: " << __VERSION__ << ", native backend: " << nativeBackendStatus() << "\n\n";
    std::string csvPath = outDir + "/bench_" + (arch == "x86-64" ? std::string("x86_64") : arch) + ".csv";
    std::ofstream csv(csvPath);
    csv << "benchmark,configuration,median_seconds,best_seconds,speedup_vs_interpreter,traces,loop_body_ops,jit_share_percent,output_matches\n";
    std::ostringstream md;
    md << "| Benchmark |";
    for (auto& c : configs) md << " " << c.name << " |";
    md << "\n|---|";
    for (size_t i = 0; i < configs.size(); i++) md << "---|";
    md << "\n";
    bool allOk = true;
    for (const std::string& f : files) {
        AssembleResult ar = assembleFile(dir + "/" + f);
        if (!ar.ok()) { std::cerr << "cannot assemble " << f << "\n"; return 2; }
        std::string name = f.substr(0, f.size() - 4);
        md << "| " << name << " |";
        std::cout << name << "\n";
        Result base;
        for (size_t ci = 0; ci < configs.size(); ci++) {
            Result r = measure(ar.program, configs[ci], reps);
            if (ci == 0) base = r;
            bool ok = r.output == base.output;
            allOk &= ok;
            double sp = r.median > 0 ? base.median / r.median : 0;
            std::cout << "  " << std::left << std::setw(30) << configs[ci].name << std::right << std::fixed
                      << std::setprecision(4) << r.median << " s   " << std::setprecision(2) << std::setw(6) << sp
                      << "x   traces=" << r.traces << " loopOps=" << r.loopOps << " jit=" << std::setprecision(1)
                      << r.jitShare << "%" << (ok ? "" : "   OUTPUT MISMATCH!") << "\n";
            csv << std::defaultfloat << name << "," << configs[ci].name << "," << std::setprecision(6) << r.median << "," << r.best << ","
                << std::setprecision(3) << sp << "," << r.traces << "," << r.loopOps << "," << std::fixed << std::setprecision(1)
                << r.jitShare << "," << (ok ? "yes" : "NO") << "\n";
            md << " " << std::setprecision(3) << r.median << " s (" << std::setprecision(2) << sp << "x) |";
        }
        md << "\n";
    }
    if (!native) std::cout << "\nNOTE: native backend unavailable; 'native' rows used the IR executor.\n";
    std::cout << "\n" << md.str() << "\nCSV written to " << csvPath << "\n";
    std::cout << (allOk ? "All configurations produced identical output.\n" : "OUTPUT MISMATCH DETECTED\n");
    std::ofstream(outDir + "/bench_" + (arch == "x86-64" ? std::string("x86_64") : arch) + ".md") << md.str();
    return allOk ? 0 : 1;
}

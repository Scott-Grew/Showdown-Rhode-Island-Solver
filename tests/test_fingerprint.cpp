#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "game/leduc.hpp"
#include "solver/best_response.hpp"
#include "solver/cfr_solver.hpp"

namespace {

std::string format_double(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.17g", value);
    return buffer;
}

template <typename Solver>
void dump_solver(std::ostream& out, const cfr::game::LeducGame& game, const char* label) {
    Solver solver(game);
    int emitted = 0;
    for (int checkpoint : {100, 500, 1000}) {
        solver.run_iterations(checkpoint - emitted);
        emitted = checkpoint;
        out << label << " iter " << checkpoint << '\n';
        out << "exploitability " << format_double(cfr::solver::exploitability(game, solver.average_strategy())) << '\n';
        for (const auto& [infoset_key, action_probs] : solver.average_strategy()) {
            out << infoset_key;
            for (double probability : action_probs) out << ' ' << format_double(probability);
            out << '\n';
        }
    }
}

std::string leduc_fingerprint() {
    cfr::game::LeducGame game;
    std::ostringstream out;
    dump_solver<cfr::solver::VanillaCfr<cfr::game::LeducGame>>(out, game, "vanilla");
    dump_solver<cfr::solver::CfrPlus<cfr::game::LeducGame>>(out, game, "cfr_plus");
    return out.str();
}

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

}

TEST_CASE("leduc behavior fingerprint byte-identical to golden", "[fingerprint]") {
    const std::string golden_path = std::string(CFR_TEST_GOLDEN_DIR) + "/leduc_fingerprint.txt";
    const std::string current = leduc_fingerprint();
    const std::string golden = read_file(golden_path);
    REQUIRE_FALSE(golden.empty());
    REQUIRE(current == golden);
}

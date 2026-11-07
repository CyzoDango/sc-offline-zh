#include "../src/zh/ship_names.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>

int main(int argc, char** argv) {
    int failures = 0, checks = 0;
    auto expect = [&](bool result, const char* what) {
        ++checks;
        if (!result) { std::fprintf(stderr, "FAIL: %s\n", what); ++failures; }
    };
    expect(sco_zh::ShipMatches("AEGS_Idris_P", "伊德里斯"), "Chinese Idris");
    expect(sco_zh::ShipMatches("AEGS_Idris_P_PU_AI_UEE", "伊德里斯"), "AI variant inherits name");
    expect(sco_zh::ShipMatches("AEGS_Idris_P", "IDRIS"), "case-insensitive English");
    expect(sco_zh::ShipMatches("DRAK_Cutlass_Black", "黑弯刀"), "common alias");
    expect(sco_zh::ShipMatches("DRAK_Cutlass_Black", "弯刀 黑"), "multiword Chinese");
    expect(sco_zh::ShipMatches("AEGS_Avenger_Titan", "复仇者泰坦"), "Chinese without spaces");
    expect(sco_zh::ShipMatches("RSI_Polaris", "北极星"), "Chinese Polaris");
    expect(sco_zh::ShipMatches("AEGS_Javelin", "标枪"), "Chinese Javelin");
    expect(sco_zh::ShipMatches("RSI_Bengal_PU_AI_UEE", "孟加拉"), "Chinese Bengal");
    expect(!sco_zh::ShipMatches("AEGS_Idris_P", "北极星"), "different ships not mixed");
    expect(!sco_zh::ShipMatches("AEGS_IdrisFake", "伊德里斯"), "prefix boundary respected");
    expect(sco_zh::ShipMatches("TEST_Unknown_42", "unknown 42"), "unknown English fallback");
    expect(sco_zh::ShipText("TEST_Unknown_42").label == "TEST_Unknown_42", "unknown ID retained");
    expect(sco_zh::ShipText("AEGS_Idris_P").label.find("AEGS_Idris_P") != std::string::npos, "original ID visible");
    expect(sco_zh::ShipMatches("DRAK_Cutlass_Black", ""), "empty filter");
    expect(sco_zh::Compact("Ab_ C-D") == "abcd", "ASCII normalization");
    int ships = 0, localized = 0;
    if (argc > 1) {
        std::ifstream list(argv[1]);
        expect(list.good(), "ship list readable");
        for (std::string name; std::getline(list, name);) {
            if (!name.empty() && name.back() == '\r') name.pop_back();
            if (name.empty() || name[0] == '#') continue;
            ++ships;
            const auto& text = sco_zh::ShipText(name.c_str());
            if (text.localized) ++localized;
            expect(text.label.find(name) != std::string::npos, "every ship keeps its class identifier");
            expect(sco_zh::ShipMatches(name.c_str(), name.c_str()), "every class can find itself");
        }
    }
    std::printf("zh names: %d checks, %d failures; %d/%d ship classes have Chinese names\n", checks, failures, localized, ships);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

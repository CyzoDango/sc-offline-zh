#pragma once
// Chinese vehicle labels and aliases. Runtime class identifiers are never changed.
// Reads only the player's already-installed Chinese localization. No community
// localization file or game asset is redistributed with this source or release.
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace sco_zh {
inline std::string Lower(std::string s) {
    for (char& c : s) if (static_cast<unsigned char>(c) < 128) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
inline std::string Compact(std::string s) {
    s = Lower(std::move(s));
    s.erase(std::remove_if(s.begin(), s.end(), [](char c) { return c == ' ' || c == '_' || c == '-' || c == '\t'; }), s.end());
    return s;
}
inline bool Prefix(const std::string& cls, const std::string& base) {
    return cls.compare(0, base.size(), base) == 0 && (cls.size() == base.size() || cls[base.size()] == '_');
}
struct Alias { const char* cls; const char* name; const char* aliases; };
inline constexpr Alias kAliases[] = {
    {"VNCL_Mauler", "剜度 重拳 驱逐舰", "重拳 刻度 梵杜尔 剜度 驱逐舰 Mauler"},
    {"RSI_Skylark", "RSI 云雀", "Skylark 云雀"},
    {"RSI_Constellation_Mk5_Centaurus", "RSI 星座 半人马座 Mk5", "半人马 Centaurus 星座"},
    {"RSI_Bengal", "孟加拉", "孟加拉级 航母"},
    {"AEGS_Idris", "伊德里斯", "伊德里斯级 护卫舰"},
    {"AEGS_Javelin", "标枪", "标枪级 驱逐舰"},
    {"RSI_Polaris", "北极星", "北极星级 鱼雷艇"},
    {"DRAK_Cutlass_Black", "弯刀 黑", "黑弯刀 黑刀"},
    {"DRAK_Cutlass_Red", "弯刀 红", "红弯刀 红刀"},
    {"DRAK_Cutlass_Blue", "弯刀 蓝", "蓝弯刀 蓝刀"},
    {"DRAK_Cutter", "小刀", "小刀 Cutter"},
    {"DRAK_Corsair", "海盗船", "海盗 海盗船"},
    {"DRAK_Caterpillar", "毛虫", "毛虫"},
    {"DRAK_Buccaneer", "劫掠者", "劫掠者 海盗机"},
    {"DRAK_Herald", "信使", "信使"},
    {"DRAK_Vulture", "秃鹫", "秃鹫 打捞"},
    {"DRAK_Kraken", "海妖", "海妖 航母"},
    {"DRAK_Mule", "骡子", "骡子 地面车"},
    {"AEGS_Avenger_Titan", "复仇者 泰坦", "泰坦 复仇者泰坦"},
    {"AEGS_Avenger_Stalker", "复仇者 追猎", "追猎 追猎者"},
    {"AEGS_Avenger_Warlock", "复仇者 术士", "术士"},
    {"AEGS_Gladius", "短剑", "短剑"},
    {"AEGS_Sabre", "军刀", "军刀"},
    {"AEGS_Hammerhead", "锤头鲨", "锤头 锤头鲨"},
    {"AEGS_Redeemer", "救赎", "救赎 救赎者"},
    {"AEGS_Reclaimer", "回收者", "回收者 打捞"},
    {"AEGS_Retaliator", "报复者", "报复 报复者"},
    {"AEGS_Vanguard", "先锋", "先锋"},
    {"AEGS_Eclipse", "日蚀", "日蚀 日食"},
    {"AEGS_Nautilus", "鹦鹉螺", "鹦鹉螺"},
    {"RSI_Constellation_Andromeda", "星座 仙女座", "仙女座 仙女"},
    {"RSI_Constellation_Taurus", "星座 金牛座", "金牛座 金牛"},
    {"RSI_Constellation_Aquila", "星座 天鹰座", "天鹰座 天鹰"},
    {"RSI_Constellation_Phoenix", "星座 凤凰座", "凤凰座 凤凰"},
    {"RSI_Aurora", "极光", "极光"},
    {"RSI_Scorpius", "天蝎", "天蝎"},
    {"RSI_Zeus", "宙斯", "宙斯"},
    {"RSI_Apollo", "阿波罗", "阿波罗"},
    {"ANVL_Carrack", "双鱼座", "双鱼座 双鱼"},
    {"ANVL_Hornet", "大黄蜂", "大黄蜂 黄蜂"},
    {"ANVL_Lightning", "闪电", "闪电 F8 F8C"},
    {"ANVL_Liberator", "解放者", "解放者"},
    {"ANVL_Paladin", "圣骑士", "圣骑士"},
    {"ANVL_Valkyrie", "女武神", "女武神"},
    {"ANVL_Terrapin", "水龟", "水龟"},
    {"MISC_Prospector", "勘探者", "勘探者 采矿"},
    {"MISC_Starfarer", "星际远航者", "远航者 星际远航者 油船"},
    {"MISC_Freelancer", "自由枪骑兵", "自由枪骑兵 枪骑兵"},
    {"MISC_Hull", "货船", "货船 HULL 船壳"},
    {"MISC_Reliant", "信赖", "信赖 信赖级"},
    {"MISC_Endeavor", "奋进", "奋进 奋进号"},
    {"CRUS_Starlifter", "大力神", "大力神 C2 M2 A2"},
    {"CRUS_Star_Runner", "星际快运", "星际快运 水星快运 MSR"},
    {"CRUS_Spirit", "精灵", "精灵 C1 A1 E1"},
    {"CRUS_Intrepid", "无畏", "无畏"},
    {"CRUS_Starfighter", "星际战斗机", "星际战斗机 战神 Ares"},
    {"ORIG_890Jump", "890 跃动", "890 890J 跃动"},
    {"ORIG_600i", "600i", "600 六百"},
    {"ORIG_400i", "400i", "400 四百"},
    {"ORIG_300i", "300i", "300 三百"},
    {"ORIG_100i", "100i", "100 一百"},
    {"ARGO_MOLE", "鼹鼠", "鼹鼠 采矿"},
    {"ARGO_RAFT", "木筏", "木筏"},
    {"ARGO_MPUV", "多用途运输艇", "MPUV 运输艇"},
    {"ARGO_ATLS", "阿特拉斯", "ATLS 动力装甲 机甲"},
    {"BANU_Merchantman", "商船", "巴努 商船 BMM"},
    {"BANU_Defender", "防卫者", "巴努 防卫者"},
    {"ESPR_Talon", "魔爪", "魔爪"},
    {"ESPR_Prowler", "潜行者", "潜行者"},
    {"GRIN_ROC", "ROC 采矿车", "ROC 洛克 采矿车"},
    {"GRIN_PTV", "PTV 地面车", "PTV 小车"},
    {"CNOU_Nomad", "游牧者", "游牧 游牧者"},
    {"CNOU_Mustang", "野马", "野马"},
    {"XIAN_Khartu", "卡图", "卡图 卡图艾尔"},
    {"GAMA_Syulen", "苏伦", "苏伦"},
    {"TMBL_Storm", "风暴", "风暴 坦克"},
    {"TMBL_Nova", "新星", "新星 坦克"}
};
struct VehicleText { std::string label, search; bool localized = false; };

class VehicleNames {
    std::unordered_map<std::string, std::string> names_;
    std::unordered_map<std::string, VehicleText> cache_;
    std::mutex lock_;
public:
    VehicleNames() {
        wchar_t file[32768] = {};
        std::filesystem::path path;
        const DWORD n = GetEnvironmentVariableW(L"SCO_ZH_GLOBAL_INI", file, static_cast<DWORD>(std::size(file)));
        if (n > 0 && n < std::size(file)) path = file;
        else if (GetModuleFileNameW(nullptr, file, static_cast<DWORD>(std::size(file))))
            path = std::filesystem::path(file).parent_path().parent_path()/L"data"/L"Localization"/L"chinese_(simplified)"/L"global.ini";
        std::ifstream input(path, std::ios::binary);
        for (std::string line; std::getline(input, line);) {
            const size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = Lower(line.substr(0, eq));
            if (key.compare(0, 12, "vehicle_name") != 0) continue;
            key.erase(0, 12);
            if (const size_t comma = key.find(','); comma != std::string::npos) key.resize(comma);
            const bool shortName = key.size() >= 6 && key.compare(key.size()-6, 6, "_short") == 0;
            if (shortName) key.resize(key.size()-6);
            std::string value = line.substr(eq+1);
            if (!value.empty() && value.back() == '\r') value.pop_back();
            if (const size_t br = value.find("\\n"); br != std::string::npos) value.resize(br);
            if (value.empty()) continue;
            if (!shortName || names_.find(key) == names_.end()) names_[key] = std::move(value);
        }
    }
    const VehicleText& Get(const char* className) {
        const std::string cls = className ? className : "";
        std::lock_guard<std::mutex> guard(lock_);
        if (const auto it = cache_.find(cls); it != cache_.end()) return it->second;
        const std::string lower = Lower(cls);
        std::string chinese, aliases;
        size_t best = 0;
        for (const auto& [key, value] : names_)
            if (key.size() > best && Prefix(lower, key)) { best = key.size(); chinese = value; }
        size_t aliasBest = 0;
        for (const Alias& alias : kAliases) {
            const std::string base = Lower(alias.cls);
            if (!Prefix(lower, base)) continue;
            aliases += " "; aliases += alias.aliases;
            if (base.size() > aliasBest) { aliasBest = base.size(); if (!best) chinese = alias.name; }
        }
        VehicleText text;
        text.localized = !chinese.empty();
        text.label = chinese.empty() ? cls : chinese + "  |  " + cls;
        text.search = Compact(text.label + " " + aliases);
        return cache_.emplace(cls, std::move(text)).first->second;
    }
};
inline const VehicleText& ShipText(const char* className) {
    static VehicleNames names;
    return names.Get(className);
}
inline bool ShipMatches(const char* className, const char* query) {
    const std::string& hay = ShipText(className).search;
    const std::string filter = query ? query : "";
    size_t from = 0;
    while (from < filter.size()) {
        from = filter.find_first_not_of(" \t", from);
        if (from == std::string::npos) break;
        const size_t end = filter.find_first_of(" \t", from);
        const std::string word = Compact(filter.substr(from, end-from));
        if (!word.empty() && hay.find(word) == std::string::npos) return false;
        if (end == std::string::npos) break;
        from = end;
    }
    return true;
}
} // namespace sco_zh

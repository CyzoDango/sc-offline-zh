#pragma once
// Presentation and search only. Never replace a game class, entity or file path.
// The selected local community localization stays on the user's computer.
#include "ship_names.h"
#include "content_catalog.generated.h"
#include <sstream>

namespace sco_zh {
enum class ContentKind { Item, Place, System, Npc, Build, Outfit, Seat, Category };
inline bool HasChinese(const std::string& value) {
    for (size_t i=0;i+2<value.size();++i) {
        const unsigned char a=value[i],b=value[i+1],c=value[i+2];
        if(a>=0xE0 && a<=0xEF && (b&0xC0)==0x80 && (c&0xC0)==0x80) {
            const unsigned cp=((a&15)<<12)|((b&63)<<6)|(c&63);
            if((cp>=0x3400 && cp<=0x9FFF)||(cp>=0xF900 && cp<=0xFAFF)) return true;
        }
    }
    return false;
}
inline std::string Trim(std::string s) {
    const size_t start=s.find_first_not_of(" \t\r\n");
    if(start==std::string::npos)return {};
    return s.substr(start,s.find_last_not_of(" \t\r\n")-start+1);
}
inline std::string Plain(std::string value) {
    // Names, not descriptions; remove inline colour/format tags.
    for(size_t pos=0;(pos=value.find('<',pos))!=std::string::npos;) {
        const size_t end=value.find('>',pos);
        if(end==std::string::npos)break;
        value.erase(pos,end-pos+1);
    }
    return Trim(std::move(value));
}
inline std::string FirstLine(const std::string& value) {
    const size_t br=value.find("\\n");
    return Plain(value.substr(0,br));
}
class LocalLexicon {
    std::unordered_map<std::string,std::string> values_,english_;
public:
    LocalLexicon() {
        wchar_t file[32768]={};std::filesystem::path path;
        const DWORD n=GetEnvironmentVariableW(L"SCO_ZH_GLOBAL_INI",file,static_cast<DWORD>(std::size(file)));
        if(n>0 && n<std::size(file))path=file;
        else if(GetModuleFileNameW(nullptr,file,static_cast<DWORD>(std::size(file))))
            path=std::filesystem::path(file).parent_path().parent_path()/L"data"/L"Localization"/L"chinese_(simplified)"/L"global.ini";
        std::ifstream input(path,std::ios::binary);
        for(std::string line;std::getline(input,line);) {
            const size_t eq=line.find('=');if(eq==std::string::npos)continue;
            std::string key=Lower(Trim(line.substr(0,eq)));
            if(const size_t comma=key.find(',');comma!=std::string::npos)key.resize(comma);
            std::string value=line.substr(eq+1);
            const std::string chinese=FirstLine(value);
            if(chinese.empty() || chinese.size()>240 || key.empty())continue;
            values_[key]=chinese;
            const bool name=key.rfind("item_name",0)==0 || key.rfind("vehicle_name",0)==0 || key.find("_name")!=std::string::npos || key.find('_')==std::string::npos;
            const size_t split=value.find("\\n");
            if(!name || split==std::string::npos || !HasChinese(chinese))continue;
            const std::string english=FirstLine(value.substr(split+2));
            if(english.empty() || english.size()>140 || HasChinese(english) || english.find('%')!=std::string::npos)continue;
            const std::string normalized=Compact(english);
            const auto [it,added]=english_.emplace(normalized,chinese);
            if(!added && it->second!=chinese)it->second.clear(); // Ambiguous English is never guessed.
        }
    }
    std::string Find(std::string key) const {
        if(!key.empty() && key[0]=='@')key.erase(0,1);
        if(const auto it=values_.find(Lower(key));it!=values_.end() && HasChinese(it->second))return it->second;
        return {};
    }
    std::string English(const std::string& text) const {
        if(const auto it=english_.find(Compact(text));it!=english_.end())return it->second;
        return {};
    }
};
inline const LocalLexicon& LocalWords() { static LocalLexicon words;return words; }
inline const char* Exact(const std::string& text) {
    const std::string key=Lower(text);
    for(const auto& pair:kContentExact)if(key==pair.from)return pair.to;
    return nullptr;
}
inline const char* Token(const std::string& text) {
    const std::string key=Lower(text);
    for(const auto& pair:kContentTokens)if(key==pair.from)return pair.to;
    if(const char* exact=Exact(text))return exact;
    return nullptr;
}
inline std::string Fallback(ContentKind kind,std::string text) {
    if(HasChinese(text))return text;
    if(const char* exact=Exact(text))return exact;
    if(const size_t slash=text.find_last_of("/\\");slash!=std::string::npos)text.erase(0,slash+1);
    if(const size_t ext=text.rfind(".socpak");ext!=std::string::npos)text.resize(ext);
    if(const size_t ext=text.rfind(".xml");ext!=std::string::npos)text.resize(ext);
    if(kind==ContentKind::Place && text.rfind("Jump point to ",0)==0)
        return "前往"+Fallback(ContentKind::System,text.substr(14))+"的跳跃点";
    std::string result,word;
    auto append=[&](const std::string& raw) {
        if(raw.empty())return;
        const std::string faction=kind==ContentKind::Npc?LocalWords().Find(raw+"_RepUI_Name"):std::string();
        if(!faction.empty()) {if(!result.empty())result+=" ";result+=faction;}
        else if(const char* translated=Token(raw)) {
            if(!*translated)return;
            if(!result.empty())result+=" ";
            result+=kind==ContentKind::Build && Lower(raw)=="light" ? "灯具" : translated;
        } else {
            // Preserve unknown abbreviations and variant numbers rather than inventing a product name.
            std::string expanded;size_t start=0;
            for(size_t i=1;i<=raw.size();++i){
                const bool boundary=i==raw.size() ||
                    (std::isupper(static_cast<unsigned char>(raw[i])) &&
                     (std::islower(static_cast<unsigned char>(raw[i-1])) || std::isdigit(static_cast<unsigned char>(raw[i-1])) ||
                      (i+1<raw.size() && std::islower(static_cast<unsigned char>(raw[i+1])))));
                if(!boundary)continue;
                const std::string part=raw.substr(start,i-start);const char* word=Token(part);
                if(!expanded.empty() && (!word || *word))expanded+=" ";expanded+=word?word:part;start=i;
            }
            if(!expanded.empty()){if(!result.empty())result+=" ";result+=expanded;}
        }
    };
    for(size_t i=0;i<=text.size();++i) {
        const char c=i<text.size()?text[i]:' ';
        if(c=='_'||c==' '||c=='-'||c=='/'||c=='\\'||c=='\t') {append(word);word.clear();}
        else word+=c;
    }
    return result.empty()?text:result;
}
struct ContentText { std::string label,search;bool community=false; };
class ContentNames {
    std::unordered_map<std::string,ContentText> cache_;
    std::mutex lock_;
public:
    const ContentText& Get(ContentKind kind,const char* raw) {
        const std::string original=raw?raw:"";
        const std::string key=std::to_string(static_cast<int>(kind))+"|"+original;
        std::lock_guard<std::mutex> guard(lock_);
        if(const auto it=cache_.find(key);it!=cache_.end())return it->second;
        ContentText value;
        const auto& local=LocalWords();
        if(kind==ContentKind::Item) {
            value.label=local.Find("item_Name_"+original);
            if(value.label.empty())value.label=local.Find("item_Name"+original);
        } else if(kind==ContentKind::Npc) {
            std::string species=Lower(original);
            if(species.rfind("vlk",0)==0)species.replace(0,3,"valakkar");
            if(species.rfind("quasigrazer",0)==0)species.replace(0,11,"quazigrazer");
            for(std::string prefix=species;!prefix.empty();){
                const std::string translated=local.Find("actor_species_creature_"+prefix);
                if(!translated.empty()){
                    value.label=translated;
                    if(prefix.size()<species.size())value.label+=" "+Fallback(kind,species.substr(prefix.size()+1));
                    break;
                }
                const size_t split=prefix.find_last_of('_');if(split==std::string::npos)break;prefix.resize(split);
            }
        } else if(kind!=ContentKind::Outfit && kind!=ContentKind::Category && kind!=ContentKind::Seat && kind!=ContentKind::Build)
            value.label=local.Find(original);
        if(value.label.empty() && kind!=ContentKind::Outfit && kind!=ContentKind::Category && kind!=ContentKind::Seat)
            value.label=local.English(original);
        value.community=!value.label.empty();
        if(value.label.empty())value.label=Fallback(kind,original);
        value.search=Compact(value.label+" "+original);
        return cache_.emplace(key,std::move(value)).first->second;
    }
};
inline const ContentText& NameText(ContentKind kind,const char* raw) { static ContentNames names;return names.Get(kind,raw); }
inline bool ContentMatches(ContentKind kind,const char* raw,const char* query) {
    const std::string& hay=NameText(kind,raw).search;
    std::istringstream stream(query?query:"");
    for(std::string word;stream>>word;)if(hay.find(Compact(word))==std::string::npos)return false;
    return true;
}
inline std::string PlaceLabel(const char* name,const char* entity) {
    // Planet/moon keys in this community dictionary are Stanton1, Pyro5a, etc.
    const std::string id=entity?entity:"";
    const std::string low=Lower(id);
    if(low.rfind("ooc_",0)==0) {
        const size_t first=low.find('_',4),second=first==std::string::npos?first:low.find('_',first+1);
        if(first!=std::string::npos && second!=std::string::npos) {
            const std::string body=low.substr(first+1,second-first-1);
            if(!body.empty() && std::isdigit(static_cast<unsigned char>(body[0]))) {
                const std::string translated=LocalWords().Find(low.substr(4,first-4)+body);
                if(!translated.empty())return translated;
            }
        }
    }
    const std::string direct=LocalWords().Find(id);
    return direct.empty()?NameText(ContentKind::Place,name).label:direct;
}
} // namespace sco_zh

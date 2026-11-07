#include "../src/zh/content_names.h"
#include "../src/zh/session_setting.h"
#include <cstdio>
#include <cstdlib>

static bool ValidUtf8(const std::string& s){
    for(size_t i=0;i<s.size();){
        const unsigned char c=s[i++];unsigned more=0;
        if(c<128)continue;
        if(c>=0xC2 && c<=0xDF)more=1;else if(c>=0xE0 && c<=0xEF)more=2;else if(c>=0xF0 && c<=0xF4)more=3;else return false;
        if(i+more>s.size())return false;
        for(unsigned n=0;n<more;++n)if((static_cast<unsigned char>(s[i++])&0xC0)!=0x80)return false;
    }return true;
}
int main(int argc,char** argv){
    using namespace sco_zh;int checks=0,failures=0;
    const auto expect=[&](bool ok,const char* what){++checks;if(!ok){++failures;std::fprintf(stderr,"FAIL: %s\n",what);}};
    expect(NameText(ContentKind::System,"Stanton").label=="斯坦顿","Stanton terminology");
    expect(NameText(ContentKind::System,"Nyx").label=="尼克斯","Nyx terminology");
    expect(ContentMatches(ContentKind::Npc,"PU_Human_Enemy_GroundCombat_NPC_CitizensforProsperity_juggernaut_sentry","繁荣公民 重装 哨兵"),"Chinese NPC faction and role");
    expect(ContentMatches(ContentKind::Npc,"PU_Human_Enemy_GroundCombat_NPC_ASD_sniper_Elite","ASD 精英 狙击"),"Chinese NPC multiword");
    expect(ContentMatches(ContentKind::Build,"ObjectContainers/PU/outlaw_outpost_a.socpak","前哨 不法者"),"Chinese build tokens");
    expect(ContentMatches(ContentKind::Build,"ObjectContainers/PU/outlaw_outpost_a.socpak","OUTLAW_OUTPOST"),"original build ID still searchable");
    expect(NameText(ContentKind::Category,"outpost parts").label=="前哨站部件","category translated");
    expect(NameText(ContentKind::Outfit,"Navy pilot flight suit 2").label=="海军飞行员飞行服 2","outfit variant preserved");
    expect(ContentMatches(ContentKind::Seat,"SCItem Seat CoPilot","副驾驶"),"Chinese seat");
    expect(!ContentMatches(ContentKind::Npc,"ASD sniper","医疗兵"),"no unrelated NPC match");
    const char stable[]="ccc_medium_armor_core_01_01_01";
    const std::string before=stable;NameText(ContentKind::Item,stable);
    expect(before==stable,"original equipment identifier immutable");
    std::printf("Samples: %s | %s | %s | %s\n",NameText(ContentKind::Item,stable).label.c_str(),
        NameText(ContentKind::Npc,"PU_Human_Enemy_GroundCombat_NPC_CitizensforProsperity_juggernaut_sentry").label.c_str(),
        NameText(ContentKind::Build,"ObjectContainers/PU/outlaw_outpost_a.socpak").label.c_str(),
        NameText(ContentKind::Outfit,"Navy pilot flight suit 2").label.c_str());
    if(argc>2 && std::string(argv[2])=="--community"){
        expect(NameText(ContentKind::Item,stable).community,"read installed community item key");
        expect(ContentMatches(ContentKind::Item,stable,"飞鸟 胸甲"),"community item terminology");
        expect(NameText(ContentKind::Item,stable).label.find("\\n")==std::string::npos,"bilingual newline trimmed");
        expect(HasChinese(PlaceLabel("microTech","OOC_Stanton_4_Microtech")),"community planet key");
    }
    // A failed write must retain the original snapshot until a successful restore.
    for(float initial:{0.0f,1.0f,3.5f}){
        float value=initial;SessionSetting setting;
        auto read=[&](float& out){out=value;return true;};
        auto write=[&](float want){value=want;return true;};
        expect(setting.Set(true,0,read,write)&&value==0,"apply session override");
        expect(setting.Set(true,0,read,write),"reapply keeps original");
        expect(setting.Set(false,0,read,write)&&value==initial&&!setting.Saved(),"restore exact original including nonboolean values");
        auto fail=[&](float want){value=want;return false;};
        expect(!setting.Set(true,1,read,fail)&&setting.Saved(),"failed write keeps recovery snapshot");
        expect(setting.Set(false,1,read,write)&&value==initial,"recover from partial write");
    }
    if(argc>1){
        struct List {const char* file;ContentKind kind;};
        const List lists[]={{"items.txt",ContentKind::Item},{"npcs.txt",ContentKind::Npc},{"buildables.txt",ContentKind::Build},{"outfits.txt",ContentKind::Outfit}};
        for(const auto& list:lists){
            std::ifstream stream(std::filesystem::path(argv[1])/list.file);expect(stream.good(),"data list readable");
            int total=0,chinese=0,community=0,printed=0;
            for(std::string id;std::getline(stream,id);){
                id=Trim(id);if(id.empty()||id[0]=='#')continue;
                if(list.kind==ContentKind::Outfit){if(id[0]!='[')continue;id=id.substr(1,id.size()-2);}
                else if(id[0]=='[')continue;
                ++total;const auto& text=NameText(list.kind,id.c_str());
                if(HasChinese(text.label))++chinese;else if(printed++<8 || std::getenv("SCO_ZH_DUMP_MISSING"))std::printf("UNMAPPED %s: %s\n",list.file,id.c_str());
                if(text.community)++community;
                expect(!text.label.empty()&&ValidUtf8(text.label),"valid untruncated UTF-8 label");
                expect(ContentMatches(list.kind,id.c_str(),id.c_str()),"original identifier finds itself");
            }
            std::printf("%s: %d/%d Chinese labels, %d direct community mappings\n",list.file,chinese,total,community);
        }
    }
    std::printf("zh content: %d checks, %d failures\n",checks,failures);return failures?EXIT_FAILURE:EXIT_SUCCESS;
}

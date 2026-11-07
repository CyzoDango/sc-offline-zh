#include <windows.h>
#include "../src/zh/start_spawn_patch.h"
#include "../src/zh/start_location.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

int wmain(int argc,wchar_t** argv){
    if(argc!=2){puts("usage: sco-start-check <StarCitizen.exe>; reads the file, never executes it");return 2;}
    std::ifstream f(std::filesystem::path(argv[1]),std::ios::binary);
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)),{});
    if(raw.size()<sizeof(IMAGE_DOS_HEADER))return 2;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(raw.data());
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||size_t(dos->e_lfanew)+sizeof(IMAGE_NT_HEADERS64)>raw.size())return 2;
    const auto* pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(raw.data()+dos->e_lfanew);
    if(pe->Signature!=IMAGE_NT_SIGNATURE||pe->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||pe->OptionalHeader.SizeOfImage>0x60000000)return 2;
    std::vector<uint8_t> mapped(pe->OptionalHeader.SizeOfImage);
    sco::Section text{},rdata{};const auto*sec=IMAGE_FIRST_SECTION(pe);
    if(reinterpret_cast<const uint8_t*>(sec+pe->FileHeader.NumberOfSections)>raw.data()+raw.size())return 2;
    for(int i=0;i<pe->FileHeader.NumberOfSections;++i){
        const auto&s=sec[i];
        if(size_t(s.PointerToRawData)+s.SizeOfRawData>raw.size()||size_t(s.VirtualAddress)+s.SizeOfRawData>mapped.size())return 2;
        std::copy_n(raw.data()+s.PointerToRawData,s.SizeOfRawData,mapped.data()+s.VirtualAddress);
        sco::Section section{mapped.data()+s.VirtualAddress,s.SizeOfRawData};
        if(!memcmp(s.Name,".text",6))text=section;if(!memcmp(s.Name,".rdata",7))rdata=section;
    }
    int checks=0,failures=0;auto expect=[&](bool ok,const char* label){++checks;if(!ok){++failures;printf("FAIL %s\n",label);}};
    auto found=sco_start::LocateSpawnDefault(text,rdata);
    expect(found.instruction && found.matches==1,"unique native spawn cvar registration");
    if(!found.instruction){printf("refused: %s\n",found.reason);return 1;}
    auto*site=found.instruction;std::array<uint8_t,7> original{};memcpy(original.data(),site,7);
    expect(*sco::RipTarget(site,3,7)==0,"original native default is empty");
    memcpy(site,found.replacement.data(),7);
    expect(sco::RipTarget(site,3,7)==found.name,"replacement points to the exact native city literal");
    expect(site[0]==0x4c&&site[1]==0x8d&&site[2]==0x0d,"instruction and register preserved");
    memcpy(site,original.data(),7);
    site[40]=9;expect(!sco_start::LocateSpawnDefault(text,rdata).instruction,"changed registration flags refused");site[40]=8;
    const auto saved=site[32];site[32]^=0x80;expect(!sco_start::LocateSpawnDefault(text,rdata).instruction,"wrong cvar reference refused");site[32]=saved;
    expect(!sco_start::LocateSpawnDefault(text,{}).instruction,"missing read-only section refused");
    for(const auto& place:sco_map::kPlaces){
        auto destination=sco_start::LocateSpawnDefault(text,rdata,place.key);
        expect(destination.instruction==site&&destination.matches==1,"native destination retains unique registration");
        if(destination.name){
            memcpy(site,destination.replacement.data(),7);
            expect(sco::RipTarget(site,3,7)==destination.name&&!strcmp(reinterpret_cast<const char*>(destination.name),place.key),"embedded native destination resolves exactly");
            memcpy(site,original.data(),7);
        }else{
            expect(destination.reason&&!strcmp(destination.reason,"destination needs a near-memory string"),"external native destination requires owned near memory");
            // Supply an unused part of this read-only test image as a synthetic near
            // allocation. Only this vector is modified; the executable stays untouched.
            auto*nearText=mapped.data()+mapped.size()-128;
            memcpy(nearText,place.key,strlen(place.key)+1);
            auto resolved=sco_start::LocateSpawnDefault(text,rdata,place.key,nearText);
            expect(resolved.name==nearText&&!resolved.reason,"owned near string produces a valid relative instruction");
            memcpy(site,resolved.replacement.data(),7);
            expect(sco::RipTarget(site,3,7)==nearText,"relative destination is the supplied near string");
            memcpy(site,original.data(),7);
        }
    }
    expect(sco_start::OverridesSavedSpot("newbabbage") && sco_start::OverridesSavedSpot("Daymar"),"explicit start preserves priority");
    expect(!sco_start::OverridesSavedSpot("") && !sco_start::OverridesSavedSpot("NewBabbage +quit"),"default and unknown inputs do not force a start");
    expect(!memcmp(site,original.data(),7),"original mapped bytes restored after checks");
    printf("native spawn: %d checks, %d failures; registration RVA 0x%llx; no game or disk writes\n",checks,failures,static_cast<unsigned long long>(site-mapped.data()));
    return failures?1:0;
}

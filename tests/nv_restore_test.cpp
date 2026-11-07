#include "../launcher/nv_smooth_motion.h"
#include <cstdio>
#include <filesystem>

int wmain(int argc, wchar_t** argv) {
    if(argc!=3 || std::wcscmp(argv[1],L"--roundtrip")!=0){std::fprintf(stderr,"Usage: sco-nv-check --roundtrip <new snapshot path> (close the game first)\n");return 2;}
    const std::wstring path=argv[2];
    std::string why;
    bool prepared=false;
    try {
        sco_nv::Setting original{};
        {
            sco_nv::Session session;
            if(!session.Available()){std::puts("NVAPI unavailable: no change");return 0;}
            sco_nv::Session::Check(session.Read(original),"read original");
        }
        std::printf("Original Smooth Motion: %u\n",original.currentValue.dword);
        bool needed=false;
        if(!sco_nv::Prepare(path,needed,why)) throw std::runtime_error(why);
        if(!needed){std::puts("Already off: no change");return 0;}
        prepared=true;
        // CREATE_NEW must protect the only original snapshot against overwrite.
        bool second=false;std::string duplicate;
        if(sco_nv::Prepare(path,second,duplicate)) throw std::runtime_error("Duplicate backup was not refused");
        if(!sco_nv::Apply(path,why)) throw std::runtime_error(why);
        sco_nv::Setting disabled{};
        {
            sco_nv::Session session;
            sco_nv::Session::Check(session.Read(disabled),"read disabled from a fresh session");
        }
        if(disabled.currentValue.dword!=0) throw std::runtime_error("Setting is not disabled");
        if(!sco_nv::Restore(path,why)) throw std::runtime_error(why);
        prepared=false;
        sco_nv::Setting restored{};
        {
            sco_nv::Session session;
            sco_nv::Session::Check(session.Read(restored),"read restored from a fresh session");
        }
        if(restored.currentValue.dword!=original.currentValue.dword || restored.location!=original.location || restored.predefined!=original.predefined)
            throw std::runtime_error("Original setting attributes were not restored");
        if(std::filesystem::exists(path)) throw std::runtime_error("Completed snapshot was left behind");
        std::printf("NVAPI roundtrip passed: %u -> 0 -> %u; duplicate backup refused; snapshot removed\n",original.currentValue.dword,restored.currentValue.dword);
        return 0;
    }catch(const std::exception& e){
        std::fprintf(stderr,"FAIL: %s\n",e.what());
        if(prepared && !sco_nv::Restore(path,why)) std::fprintf(stderr,"RESTORE REQUIRED: %s\n",why.c_str());
        return 1;
    }
}

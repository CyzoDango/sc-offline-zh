#pragma once
// Local zh.1 compatibility fix. Changes ONLY the StarCitizen.exe DRS profile.
// An exact, flushed snapshot is saved before a change and restored after play.
// This replaces the local prototype's Python dependency; no NVAPI DLL is bundled.
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <stdexcept>
#include <string>

namespace sco_nv {
constexpr uint32_t kSettingId = 0xB0D384C0;
union Value { uint32_t dword; unsigned char raw[4100]; };
struct Setting {
    uint32_t version; wchar_t name[2048];
    uint32_t id, type, location, predefined, predefinedValid;
    Value predefinedValue, currentValue;
};
struct Application {
    uint32_t version, predefined;
    wchar_t name[2048], friendly[2048], launcher[2048], folder[2048];
    uint32_t flags;
    wchar_t commandline[2048];
};
struct Profile { uint32_t version; wchar_t name[2048]; uint32_t gpu, predefined, apps, settings; };
static_assert(sizeof(Setting) == 12320 && sizeof(Application) == 20492 && sizeof(Profile) == 4116);
struct Snapshot {
    uint32_t magic = 0x564E4353, format = 1, restoreWithSet = 0;
    int32_t status = 0;
    wchar_t profile[2048] = {};
    Setting setting = {};
    uint32_t checksum = 0;
};
inline uint32_t Checksum(const Snapshot& s) {
    uint32_t h = 2166136261u;
    const auto* bytes = reinterpret_cast<const unsigned char*>(&s);
    for (size_t i = 0; i < offsetof(Snapshot, checksum); ++i) h = (h ^ bytes[i]) * 16777619u;
    return h;
}
class Session {
    HMODULE dll_ = nullptr;
    void* session_ = nullptr;
    using Query = void* (__cdecl*)(uint32_t);
    Query query_ = nullptr;
    using Handle = void*;
    template<class F> F Bind(uint32_t id) {
        void* fn = query_(id);
        if (!fn) throw std::runtime_error("NVAPI interface unavailable");
        return reinterpret_cast<F>(fn);
    }
    using Destroy = int (__cdecl*)(Handle);
    Destroy destroy_ = nullptr;
public:
    Handle profile = nullptr;
    Profile info{};
    Session() {
        wchar_t system[MAX_PATH] = {};
        GetSystemDirectoryW(system, MAX_PATH);
        const std::wstring path = std::wstring(system) + L"\\nvapi64.dll";
        dll_ = LoadLibraryW(path.c_str());
        if (!dll_) return; // AMD/Intel machines have nothing to change.
        try {
            query_ = reinterpret_cast<Query>(GetProcAddress(dll_, "nvapi_QueryInterface"));
            if (!query_) throw std::runtime_error("NVAPI query interface missing");
            Check(Bind<int(__cdecl*)()>(0x0150E828)(), "initialize");
            destroy_ = Bind<Destroy>(0xDAD9CFF8);
            Check(Bind<int(__cdecl*)(Handle*)>(0x0694D52E)(&session_), "create session");
            Reload();
            Application app{}; app.version = uint32_t(sizeof(app)) | (4u << 16);
            const int rc = Bind<int(__cdecl*)(Handle,const wchar_t*,Handle*,Application*)>(0xEEE566B2)(session_, L"StarCitizen.exe", &profile, &app);
            if (rc == -6 || rc == -163) { profile = nullptr; return; }
            Check(rc, "find StarCitizen.exe profile");
            info.version = uint32_t(sizeof(info)) | (1u << 16);
            Check(Bind<int(__cdecl*)(Handle,Handle,Profile*)>(0x61CD6FD6)(session_,profile,&info), "read profile");
        } catch (...) { Close(); throw; }
    }
    ~Session() { Close(); }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    bool Available() const { return profile != nullptr; }
    static void Check(int rc, const char* operation) {
        if (rc) throw std::runtime_error(std::string("NVAPI ")+operation+" returned "+std::to_string(rc));
    }
    void Close() { if(session_ && destroy_) destroy_(session_); session_=nullptr; if(dll_) FreeLibrary(dll_); dll_=nullptr; }
    void Reload() { Check(Bind<int(__cdecl*)(Handle)>(0x375DBD6B)(session_), "load settings"); }
    int Read(Setting& value) {
        value = {}; value.version = uint32_t(sizeof(value)) | (1u << 16); value.id = kSettingId;
        uint32_t extra = 0;
        return Bind<int(__cdecl*)(Handle,Handle,uint32_t,Setting*,uint32_t*)>(0xEA99498D)(session_,profile,kSettingId,&value,&extra);
    }
    void Set(Setting& value) { Check(Bind<int(__cdecl*)(Handle,Handle,Setting*,uint32_t,uint32_t)>(0x8A2CF5F5)(session_,profile,&value,0,0), "set setting"); }
    void Remove() { const int rc=Bind<int(__cdecl*)(Handle,Handle,uint32_t)>(0xD20D29DF)(session_,profile,kSettingId); if(rc != -160) Check(rc,"remove temporary setting"); }
    void Save() { Check(Bind<int(__cdecl*)(Handle)>(0xFCBC7E14)(session_), "save settings"); Reload(); }
};
inline bool ReadSnapshot(const std::wstring& path, Snapshot& value, std::string& why) {
    HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(f==INVALID_HANDLE_VALUE){why="NVIDIA snapshot cannot be opened";return false;}
    LARGE_INTEGER size{}; DWORD got=0;
    const bool ok=GetFileSizeEx(f,&size) && size.QuadPart==sizeof(value) && ReadFile(f,&value,sizeof(value),&got,nullptr) && got==sizeof(value);
    CloseHandle(f);
    if(!ok || value.magic!=0x564E4353 || value.format!=1 || value.setting.id!=kSettingId || value.checksum!=Checksum(value)){
        why="NVIDIA snapshot is incomplete or invalid";return false;
    }
    value.profile[2047]=0;
    return true;
}
// Snapshot only: the caller must durably record ownership before applying it.
inline bool Prepare(const std::wstring& path, bool& needed, std::string& why) {
    needed=false;
    try {
        if(GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES) throw std::runtime_error("An NVIDIA snapshot already needs restoration");
        Session nv;
        if(!nv.Available()) return true;
        Snapshot before{}; before.status=nv.Read(before.setting);
        if(before.status==-160) return true;
        Session::Check(before.status,"read Smooth Motion");
        if(before.setting.type!=0) throw std::runtime_error("Smooth Motion is not a DWORD setting");
        if(before.setting.currentValue.dword==0) return true;
        before.restoreWithSet=before.setting.location==0 && before.setting.predefined==0;
        std::memcpy(before.profile,nv.info.name,sizeof(before.profile));
        before.checksum=Checksum(before);
        HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
        if(f==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create the NVIDIA snapshot");
        DWORD written=0;
        const bool ok=WriteFile(f,&before,sizeof(before),&written,nullptr) && written==sizeof(before) && FlushFileBuffers(f);
        CloseHandle(f);
        if(!ok){DeleteFileW(path.c_str());throw std::runtime_error("Cannot flush the NVIDIA snapshot");}
        needed=true; return true;
    }catch(const std::exception& e){why=e.what();return false;}
}
inline bool Apply(const std::wstring& path, std::string& why) {
    try {
        Snapshot before{}; if(!ReadSnapshot(path,before,why)) return false;
        Session nv;
        if(!nv.Available() || std::wcscmp(before.profile,nv.info.name)!=0) throw std::runtime_error("NVIDIA profile identity changed");
        Setting value{}; value.version=uint32_t(sizeof(value))|(1u<<16);value.id=kSettingId;value.currentValue.dword=0;
        nv.Set(value);nv.Save();
        Session::Check(nv.Read(value),"verify Smooth Motion");
        if(value.currentValue.dword!=0) throw std::runtime_error("Smooth Motion did not turn off");
        return true;
    }catch(const std::exception& e){why=e.what();return false;}
}
inline bool Restore(const std::wstring& path, std::string& why, bool removeSnapshot = true) {
    try {
        Snapshot before{}; if(!ReadSnapshot(path,before,why)) return false;
        Session nv;
        if(!nv.Available() || std::wcscmp(before.profile,nv.info.name)!=0) throw std::runtime_error("NVIDIA profile identity changed");
        if(before.restoreWithSet) nv.Set(before.setting); else nv.Remove();
        nv.Save();Setting verified{};const int rc=nv.Read(verified);
        if(rc!=before.status || (rc==0 && (verified.currentValue.dword!=before.setting.currentValue.dword || verified.location!=before.setting.location || verified.predefined!=before.setting.predefined)))
            throw std::runtime_error("Restored NVIDIA setting differs from the original");
        if(removeSnapshot && !DeleteFileW(path.c_str())) throw std::runtime_error("Restored NVIDIA snapshot could not be removed");
        return true;
    }catch(const std::exception& e){why=e.what();return false;}
}
} // namespace sco_nv

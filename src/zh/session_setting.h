#pragma once
// A runtime-only override. Capture the original before writing; keep it until
// restoration is read back successfully, including when the first write fails.
namespace sco_zh {
class SessionSetting {
    float original_=0;
    bool saved_=false,enabled_=false;
public:
    bool Saved()const{return saved_;}
    bool Enabled()const{return enabled_;}
    template<class Read,class Write> bool Set(bool on,float wanted,Read read,Write write) {
        float actual=0;
        if(on) {
            if(!saved_) { if(!read(original_))return false;saved_=true; }
            if(!write(wanted)||!read(actual)||actual!=wanted)return false;
            enabled_=true;return true;
        }
        if(!saved_){enabled_=false;return true;}
        if(!write(original_)||!read(actual)||actual!=original_)return false;
        saved_=false;enabled_=false;return true;
    }
};
} // namespace sco_zh

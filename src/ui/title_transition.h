#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace sco_v3 {
// Exact final-design motion: easeOutCubic, 0.2 seconds, +/-6 logical pixels.
// Pure display state: no config writes and no game calls.
class TitleTransition {
public:
    struct Layer {std::string text;float alpha=1,offset=0;};
    static constexpr double Duration=.2;
    bool Set(const std::string& text,double now){
        if(initialized_&&text==target_)return false;
        if(!initialized_){initialized_=true;target_=text;return true;}
        auto visible=Sample(now);
        visible.erase(std::remove_if(visible.begin(),visible.end(),[](const Layer&l){return l.alpha<=.0001f;}),visible.end());
        // Keep rapid repeated selection bounded while preserving the visible blend.
        if(visible.size()>3){
            std::stable_sort(visible.begin(),visible.end(),[](const Layer&a,const Layer&b){return a.alpha>b.alpha;});
            visible.resize(3);float sum=0;for(const auto&l:visible)sum+=l.alpha;
            if(sum>0)for(auto&l:visible)l.alpha/=sum;
        }
        outgoing_=std::move(visible);target_=text;started_=now;return true;
    }
    bool Active(double now)const{return initialized_&&!outgoing_.empty()&&now<started_+Duration;}
    std::vector<Layer> Sample(double now)const{
        if(!initialized_)return {};
        if(!Active(now))return {{target_,1,0}};
        const double t=(std::clamp)((now-started_)/Duration,0.,1.);
        const float e=float(1-(1-t)*(1-t)*(1-t));
        std::vector<Layer> layers;layers.reserve(outgoing_.size()+1);
        for(const auto&l:outgoing_)layers.push_back({l.text,l.alpha*(1-e),l.offset-6*e});
        layers.push_back({target_,e,6*(1-e)});return layers;
    }
private:
    bool initialized_=false;double started_=0;
    std::string target_;std::vector<Layer>outgoing_;
};
}

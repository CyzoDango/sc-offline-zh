#include "../src/ui/title_transition.h"
#include <cstdio>
#include <cmath>
int main(){
    int checks=0,failures=0;auto expect=[&](bool condition,const char*name){++checks;if(!condition){++failures;std::printf("FAIL %s\n",name);}};
    auto close=[](float a,float b){return std::abs(a-b)<.0001f;};
    sco_v3::TitleTransition animation;
    expect(animation.Sample(0).empty(),"no initial ghost label");
    animation.Set("Lorville",0);auto layers=animation.Sample(0);
    expect(layers.size()==1&&close(layers[0].alpha,1)&&close(layers[0].offset,0)&&!animation.Active(0),"initial selection is steady");
    expect(!animation.Set("Lorville",1.5),"periodic refresh does not restart");
    animation.Set("Orison",2);layers=animation.Sample(2);
    expect(layers.size()==2&&layers[0].text=="Lorville"&&layers[1].text=="Orison","old/new title retained");
    expect(close(layers[0].alpha,1)&&close(layers[1].alpha,0)&&close(layers[1].offset,6),"incoming begins below with zero opacity");
    layers=animation.Sample(2.1);
    expect(close(layers[0].alpha,.125f)&&close(layers[1].alpha,.875f),"0.2 second easeOutCubic");
    expect(close(layers[0].offset,-5.25f)&&close(layers[1].offset,.75f),"six-pixel upward motion");
    const auto before=layers;animation.Set("New Babbage",2.1);layers=animation.Sample(2.1);
    expect(layers.size()==3&&close(layers[0].alpha,before[0].alpha)&&close(layers[1].alpha,before[1].alpha)&&close(layers[0].offset,before[0].offset)&&close(layers[1].offset,before[1].offset),"interruption preserves currently visible layers");
    animation.Set("New Babbage",2.15);expect(!animation.Active(2.31),"same label never prolongs transition");
    layers=animation.Sample(2.31);expect(layers.size()==1&&layers[0].text=="New Babbage"&&close(layers[0].alpha,1)&&close(layers[0].offset,0),"settles at the latest label");
    for(int i=0;i<100;++i){const double now=3+i*.002;animation.Set(std::to_string(i),now);layers=animation.Sample(now);float sum=0;for(const auto&l:layers)sum+=l.alpha;expect(layers.size()<=4&&close(sum,1),"rapid switching bounded and opaque");}
    expect(animation.Sample(10).size()==1&&!animation.Active(10),"old layers stop rendering");
    std::printf("Title transition: %d checks, %d failures; no GUI/game launched\n",checks,failures);return failures?1:0;
}

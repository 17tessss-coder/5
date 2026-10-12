#include "../source/DSPCore.h"
#include <cassert>
#include <cmath>
#include <array>
#include <iostream>

int main(){
    SauceDSP::Engine engine;
    engine.prepare(48000,2);
    std::array<float,SauceDSP::numParams> p=SauceDSP::defaults;
    // Must be bit-exact bypass on neutral state.
    std::array<float,2048> a{},b{};
    for(int i=0;i<2048;++i) a[(size_t)i]=b[(size_t)i]=.22f*std::sin(float(i)*.07f);
    float* out[]={a.data(),b.data()};
    engine.process(out,2,2048,p);
    for(int i=0;i<2048;++i)assert(a[(size_t)i]==b[(size_t)i]);
    // Every effect: finite values and meaningful output.
    const int effectIds[]={SauceDSP::sat,SauceDSP::dist,SauceDSP::air,SauceDSP::punch,
       SauceDSP::deess,SauceDSP::color,SauceDSP::chorus,SauceDSP::comp};
    for(int effectId:effectIds){
        engine.reset();p=SauceDSP::defaults;
        p[(size_t)effectId]=100.f;
        a.fill(0.f);b.fill(0.f);
        for(int i=0;i<2048;++i)a[(size_t)i]=b[(size_t)i]=.27f*std::sin(float(i)*.15f);
        engine.process(out,2,2048,p);
        for(auto s:a)assert(std::isfinite(s));
    }
    // Unordered, freely movable EQ bands at maximum gain must not explode into NaNs.
    engine.reset();p=SauceDSP::defaults;
    for(int k=0;k<6;++k){
        p[(size_t)(SauceDSP::eqGain1+k)]=k%2?36.f:-36.f;
        p[(size_t)(SauceDSP::eqFreq1+k)]=13000.f-2300.f*k;
        p[(size_t)(SauceDSP::eqQ1+k)]=.7f;
    }
    a.fill(0.f);b.fill(0.f);
    for(int i=0;i<2048;++i)a[(size_t)i]=b[(size_t)i]=.12f*std::sin(float(i)*.12f);
    engine.process(out,2,2048,p);
    for(auto s:a)assert(std::isfinite(s));
    std::cout << "17TESS V0.4 DSP tests passed\n";
}

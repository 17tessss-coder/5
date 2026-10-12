#pragma once
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace SauceDSP {
constexpr int eqBands = 6;
constexpr int numParams = 51;
enum Id {
    sat, dist, air, punch, wet, deess, color, chorus, comp,
    satDrive, satTone, satOutput,
    distDrive, distHP, distLP, distOutput,
    airFreq, airBoost,
    punchAttack, punchRelease,
    wetOutput,
    deessFreq, deessAmount,
    colorTilt, colorBias,
    chorusRate, chorusDepth, chorusWidth,
    compThreshold, compRatio, compAttack, compRelease, compMakeup,
    eqGain1, eqGain2, eqGain3, eqGain4, eqGain5, eqGain6,
    eqFreq1, eqFreq2, eqFreq3, eqFreq4, eqFreq5, eqFreq6,
    eqQ1, eqQ2, eqQ3, eqQ4, eqQ5, eqQ6
};
inline constexpr std::array<const char*, numParams> ids {{
    "sat", "dist", "air", "punch", "wet", "deess", "color", "chorus", "comp",
    "sat_drive", "sat_tone", "sat_output",
    "dist_drive", "dist_hp", "dist_lp", "dist_output",
    "air_freq", "air_boost", "punch_attack", "punch_release", "wet_output",
    "deess_freq", "deess_amount", "color_tilt", "color_bias",
    "chorus_rate", "chorus_depth", "chorus_width",
    "comp_threshold", "comp_ratio", "comp_attack", "comp_release", "comp_makeup",
    "eq_gain1", "eq_gain2", "eq_gain3", "eq_gain4", "eq_gain5", "eq_gain6",
    "eq_freq1", "eq_freq2", "eq_freq3", "eq_freq4", "eq_freq5", "eq_freq6",
    "eq_q1", "eq_q2", "eq_q3", "eq_q4", "eq_q5", "eq_q6"
}};
inline constexpr std::array<float, numParams> defaults {{
    0.f,0.f,0.f,0.f,100.f,0.f,0.f,0.f,0.f,
    60.f,50.f,0.f,60.f,65.f,15000.f,0.f,4400.f,55.f,
    90.f,9.f,0.f,6600.f,50.f,0.f,50.f,0.6f,40.f,50.f,
    -18.f,4.f,12.f,80.f,0.f,
    0.f,0.f,0.f,0.f,0.f,0.f,
    60.f,160.f,650.f,1800.f,5500.f,12000.f,
    1.f,1.f,1.f,1.f,1.f,1.f
}};
inline double clamp(double x,double a,double b) noexcept {return std::min(b,std::max(a,x));}
inline double dbGain(double db) noexcept {return std::pow(10.,db/20.);}
inline double coeffHz(double hz,double sr) noexcept {return 1.-std::exp(-6.283185307179586*clamp(hz,1.,sr*.45)/sr);}
inline double coeffMs(double ms,double sr) noexcept {return 1.-std::exp(-1./(std::max(ms,0.01)*.001*sr));}
inline double softclip(double x) noexcept {return std::tanh(clamp(x,-24.,24.));}

struct Biquad {
    double z1=0.,z2=0.;
    void reset() noexcept {z1=z2=0.;}
    double process(double x, const std::array<double,5>& a) noexcept {
        const double y=a[0]*x+z1;
        z1=a[1]*x-a[3]*y+z2;
        z2=a[2]*x-a[4]*y;
        return std::isfinite(y)?clamp(y,-100.,100.):0.;
    }
};
inline std::array<double,5> peaking(double hz,double db,double q,double sr) noexcept {
    hz=clamp(hz,30.,sr*.45); q=clamp(q,.25,10.);
    const double w=6.283185307179586*hz/sr;
    const double a=std::pow(10.,db/40.);
    const double alpha=std::sin(w)/(2.*q),cosw=std::cos(w);
    const double div=1.+alpha/a;
    return {{(1.+alpha*a)/div,(-2.*cosw)/div,(1.-alpha*a)/div,
             (-2.*cosw)/div,(1.-alpha/a)/div}};
}
struct Channel {
    double hp=0.,lp=0.,airLP=0.,deessLP=0.,deessEnvelope=0.;
    double fast=0.,slow=0.,colorLow=0.,colorMid=0.,compEnv=0.,satToneLP=0.;
    double chorusPhase=0.;
    size_t index=0;
    std::vector<float> delay;
    std::array<Biquad,eqBands> eq{};
    void reset() noexcept {
        hp=lp=airLP=deessLP=deessEnvelope=fast=slow=colorLow=colorMid=compEnv=satToneLP=chorusPhase=0.;
        index=0;for(auto& q:eq)q.reset();std::fill(delay.begin(),delay.end(),0.f);
    }
};
class Engine {
public:
    void prepare(double newRate,int channels) {
        sr=std::max(8000.,newRate);usedChannels=std::clamp(channels,1,2);
        for(auto& c:states){c.delay.assign((size_t)std::ceil(sr*.085)+8,0.f);c.reset();}
    }
    void reset() noexcept {for(auto& c:states)c.reset();}
    void process(float* const* channels,int channelCount,int samples,const std::array<float,numParams>& p) noexcept {
        if(!channels||samples<=0)return;
        const int number=std::min({channelCount,usedChannels,2});
        const double sAmt=clamp(p[sat]/100.,0.,1.),dAmt=clamp(p[dist]/100.,0.,1.);
        const double aAmt=clamp(p[air]/100.,0.,1.),pnAmt=clamp(p[punch]/100.,0.,1.);
        const double deAmt=clamp(p[deess]/100.,0.,1.),coAmt=clamp(p[color]/100.,0.,1.);
        const double chAmt=clamp(p[chorus]/100.,0.,1.),cpAmt=clamp(p[comp]/100.,0.,1.);
        const double wetAmt=clamp(p[wet]/100.,0.,1.),trim=dbGain(p[wetOutput]);
        bool eqOn=false;
        std::array<std::array<double,5>,eqBands> filters{};
        for(int b=0;b<eqBands;++b){
            const double gain=p[eqGain1+b];eqOn|=std::abs(gain)>.0001;
            filters[(size_t)b]=peaking(p[eqFreq1+b],gain,p[eqQ1+b],sr);
        }
        const bool hasFx=sAmt>0||dAmt>0||aAmt>0||pnAmt>0||deAmt>0||coAmt>0||chAmt>0||cpAmt>0||eqOn;
        if(!hasFx&&std::abs(trim-1.)<1.e-8)return;
        const double dHP=coeffHz(p[distHP],sr),dLP=coeffHz(p[distLP],sr);
        const double aCut=coeffHz(p[airFreq],sr),sTone=coeffHz(1800.+p[satTone]*110.,sr);
        const double deCut=coeffHz(p[deessFreq],sr),deEnv=coeffHz(120.,sr);
        const double pnAttack=coeffHz(p[punchAttack],sr),pnRelease=coeffHz(p[punchRelease],sr);
        const double colLow=coeffHz(280.,sr),colMid=coeffHz(2400.,sr);
        const double ca=coeffMs(p[compAttack],sr),cr=coeffMs(p[compRelease],sr);
        const double ratio=clamp(p[compRatio],1.,20.),thresh=clamp(p[compThreshold],-50.,0.);
        const double satExtra=clamp(p[satDrive]/60.,0.,2.),distExtra=clamp(p[distDrive]/60.,0.,2.);
        const double rate=clamp(p[chorusRate],.05,5.),depth=clamp(p[chorusDepth]/100.,0.,1.);
        const double width=clamp(p[chorusWidth]/100.,0.,1.);
        const double phaseStep=6.283185307179586*rate/sr;
        for(int ch=0;ch<number;++ch){
            float* data=channels[ch];if(!data)continue;
            Channel& c=states[(size_t)ch];
            for(int i=0;i<samples;++i){
                const double dry=std::isfinite(data[i])?clamp(data[i],-16.,16.):0.;
                double x=dry;
                if(sAmt>0){
                    c.satToneLP+=sTone*(x-c.satToneLP);
                    const double brightness=0.5+(p[satTone]/100.);
                    const double toned=c.satToneLP+(x-c.satToneLP)*brightness;
                    const double drive=1.+3.3*sAmt*satExtra;
                    const double normalized=softclip(toned*drive)/(softclip(drive)+1.e-9);
                    const double blend=std::min(.95,1.10*sAmt);
                    x=(1.-blend)*x+blend*normalized;
                    x*=dbGain(p[satOutput]);
                }
                if(dAmt>0){
                    c.hp+=dHP*(x-c.hp);const double highPassed=x-c.hp;
                    c.lp+=dLP*(highPassed-c.lp);
                    const double driven=softclip(c.lp*(1.+35.*dAmt*distExtra))*.85;
                    x=(1.-dAmt)*x+dAmt*driven;
                    x*=dbGain(p[distOutput]);
                }
                if(aAmt>0){
                    c.airLP+=aCut*(x-c.airLP);
                    x+=(x-c.airLP)*aAmt*.55*clamp(p[airBoost]/55.,0.,2.);
                }
                if(pnAmt>0){
                    const double amp=std::abs(x);
                    c.fast+=pnAttack*(amp-c.fast);c.slow+=pnRelease*(amp-c.slow);
                    x*=1.+pnAmt*clamp(7.*(c.fast-c.slow),-.33,1.4);
                }
                if(cpAmt>0){
                    const double detector=std::abs(x);
                    const double targetDb=20.*std::log10(std::max(1.e-9,detector));
                    const double over=std::max(0.,targetDb-thresh);
                    const double targetReduction=-over*(1.-1./ratio);
                    c.compEnv+=(targetReduction<c.compEnv?ca:cr)*(targetReduction-c.compEnv);
                    const double compressed=x*dbGain(c.compEnv+p[compMakeup]);
                    x=x*(1.-cpAmt)+compressed*cpAmt;
                }
                if(deAmt>0){
                    c.deessLP+=deCut*(x-c.deessLP);const double high=x-c.deessLP;
                    c.deessEnvelope+=deEnv*(std::abs(high)-c.deessEnvelope);
                    const double red=deAmt*clamp(p[deessAmount]/50.,0.,2.)*clamp(28.*(c.deessEnvelope-.012),0.,.84);
                    x=c.deessLP+high*(1.-red);
                }
                if(coAmt>0){
                    c.colorLow+=colLow*(x-c.colorLow);
                    c.colorMid+=colMid*(x-c.colorMid);
                    const double tilt=clamp(p[colorTilt]/100.,-1.,1.);
                    x+=coAmt*(.35*tilt*(c.colorLow-(x-c.colorMid))+.08*(c.colorMid-c.colorLow)
                       *(p[colorBias]/50.));
                }
                if(eqOn){for(int b=0;b<eqBands;++b)x=c.eq[(size_t)b].process(x,filters[(size_t)b]);}
                // Chorus: 25 ms modulated delay with very short (<2 ms) displacement, weak wet mix.
                // Independent L/R phase keeps stereo width without the unstable flanger effect.
                if(chAmt>0&&c.delay.size()>8){
                    const size_t size=c.delay.size();
                    const double phase=c.chorusPhase+(ch==1?width*1.57079632679:0.);
                    const double delayMs=27.+(0.3+depth*1.7)*std::sin(phase);
                    const double offset=clamp(delayMs*.001*sr,1.,(double)size-3.);
                    const double pos=(double)c.index+(double)size-offset;
                    const size_t ix=(size_t)pos%size;
                    const size_t ix2=(ix+1)%size;
                    const double fraction=pos-std::floor(pos);
                    const double delayed=(1.-fraction)*c.delay[ix]+fraction*c.delay[ix2];
                    c.delay[c.index]=(float)x;c.index=(c.index+1)%size;
                    c.chorusPhase+=phaseStep;
                    if(c.chorusPhase>=6.283185307179586)c.chorusPhase-=6.283185307179586;
                    const double mix=.24*chAmt;
                    x=(1.-mix)*x+mix*delayed;
                }
                const double result=trim*(dry*(1.-wetAmt)+x*wetAmt);
                data[i]=(float)clamp(std::isfinite(result)?result:0.,-32.,32.);
            }
        }
    }
private:
    double sr=44100.;int usedChannels=2;
    std::array<Channel,2> states{};
};
}

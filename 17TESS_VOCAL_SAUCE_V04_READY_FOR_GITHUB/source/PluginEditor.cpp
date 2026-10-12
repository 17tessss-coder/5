#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>
#include <algorithm>

namespace {
constexpr int tileX[8] = {38,326,613,893,38,326,613,893};
constexpr int tileY[8] = {227,227,227,227,432,432,432,432};
constexpr int compX=843,compY=702;
const auto black = juce::Colour::fromRGB(12,12,12);
const auto pale = juce::Colour::fromRGB(246,246,240);
const auto gold = juce::Colour::fromRGB(254,229,33);
const auto grass = juce::Colour::fromRGB(108,235,57);
const auto stone = juce::Colour::fromRGB(100,100,100);
const auto dark = juce::Colour::fromRGB(29,29,29);
const std::array<const char*,9> mainNames{{"SATURATION","DISTORTION","AIR","PUNCH","WET","DE-ESS","COLOR EQ","CHORUS","COMPRESSOR"}};
struct Spec { int owner; const char* id; const char* name; };
const std::array<Spec,30> specs{{
  {0,"sat_drive","DRIVE"}, {0,"sat_tone","TONE"}, {0,"sat_output","OUTPUT"},
  {1,"dist_drive","DRIVE"}, {1,"dist_hp","LOW CUT"}, {1,"dist_lp","HIGH CUT"}, {1,"dist_output","OUTPUT"},
  {2,"air_freq","FREQUENCY"}, {2,"air_boost","BOOST"},
  {3,"punch_attack","ATTACK"}, {3,"punch_release","RELEASE"},
  {4,"wet_output","OUTPUT TRIM"},
  {5,"deess_freq","SIBILANCE"}, {5,"deess_amount","DEPTH"},
  {6,"color_tilt","TILT"}, {6,"color_bias","BIAS"},
  {7,"chorus_rate","RATE"}, {7,"chorus_depth","DEPTH"}, {7,"chorus_width","WIDTH"},
  {8,"comp_threshold","THRESHOLD"}, {8,"comp_ratio","RATIO"}, {8,"comp_attack","ATTACK"},
  {8,"comp_release","RELEASE"}, {8,"comp_makeup","MAKEUP"},
  {9,"eq_q1","BAND 1 Q"}, {9,"eq_q2","BAND 2 Q"}, {9,"eq_q3","BAND 3 Q"},
  {9,"eq_q4","BAND 4 Q"}, {9,"eq_q5","BAND 5 Q"}, {9,"eq_q6","BAND 6 Q"}
}};
juce::Font px(float size){
    return juce::Font(juce::Font::getDefaultMonospacedFontName(),size,juce::Font::bold);
}
void pixelText(juce::Graphics& g,const juce::String& text,juce::Rectangle<int> r,int size,juce::Colour col){
    g.setFont(px((float)size));g.setColour(black);g.drawText(text,r.translated(2,2),juce::Justification::centred,true);
    g.setColour(col);g.drawText(text,r,juce::Justification::centred,true);
}
int eqGainId(int i){return SauceDSP::eqGain1+i;}
int eqFreqId(int i){return SauceDSP::eqFreq1+i;}
}

void SauceLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,float,float,juce::Slider&) {
    const int size=(std::min)(w,h)-5;
    const int bx=x+w/2-size/2,by=y+h/2-size/2;
    g.setColour(black);g.fillRect(bx-3,by-3,size+6,size+6);
    g.setColour(juce::Colour::fromRGB(57,57,57));g.fillRect(bx,by,size,size);
    g.setColour(juce::Colour::fromRGB(169,170,170));g.fillRect(bx,by,size,5);g.fillRect(bx,by,5,size);
    g.setColour(juce::Colour::fromRGB(109,109,109));g.fillRect(bx+size-5,by,5,size);g.fillRect(bx,by+size-5,size,5);
    g.setColour(juce::Colour::fromRGB(37,37,37));g.fillRect(bx+11,by+11,size-22,size-22);
    g.setColour(black);g.fillRect(bx+size/2-4,by+size/2-4,8,8);
    const float angle=-2.35f+position*4.7f;
    const int px1=bx+size/2+(int)(std::sin(angle)*size*.29f);
    const int py1=by+size/2-(int)(std::cos(angle)*size*.29f);
    g.setColour(gold);g.fillRect(px1-5,py1-5,10,10);
}

void SauceLook::drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float pos,float,float,juce::Slider::SliderStyle,juce::Slider&) {
    const int mid=y+height/2;
    g.setColour(black);g.fillRect(x+5,mid-5,width-10,10);
    g.setColour(juce::Colour::fromRGB(38,69,27));g.fillRect(x+8,mid-2,width-16,4);
    const int px1=juce::jlimit(x+8,x+width-8,(int)pos);
    g.setColour(grass);g.fillRect(x+8,mid-2,(std::max)(0,px1-(x+8)),4);
    g.setColour(black);g.fillRect(px1-6,mid-9,12,18);
    g.setColour(gold);g.fillRect(px1-4,mid-7,8,14);
}

juce::Point<float> SauceEQGraph::point(int b) const {
    const double f=processor.param(eqFreqId(b));
    const double g=processor.param(eqGainId(b));
    const auto r=getLocalBounds().reduced(14,11).toFloat();
    return {r.getX()+(float)(std::log(f/30.)/std::log(16500./30.))*r.getWidth(),
            r.getY()+(float)((36.-g)/72.)*r.getHeight()};
}
void SauceEQGraph::paint(juce::Graphics& g){
    auto r=getLocalBounds();
    g.setColour(juce::Colour::fromRGB(0,29,12));g.fillRect(r);
    g.setColour(juce::Colour::fromRGB(12,64,27));
    for(int i=1;i<15;++i)g.fillRect(r.getWidth()*i/15,0,1,r.getHeight());
    for(int i=1;i<9;++i)g.fillRect(0,r.getHeight()*i/9,r.getWidth(),1);
    g.setColour(juce::Colour::fromRGB(88,151,68));g.fillRect(0,r.getHeight()/2,r.getWidth(),2);
    juce::Path curve;
    for(int x=0;x<r.getWidth();x+=3){
        const double f=30.*std::pow(16500./30.,double(x)/r.getWidth());
        double gain=0.;
        for(int k=0;k<SauceDSP::eqBands;++k){
            const double freq=processor.param(eqFreqId(k)),q=processor.param(SauceDSP::eqQ1+k);
            const double delta=std::log2(f/freq);
            const double width=.78/(std::max)(.25,q);
            gain+=processor.param(eqGainId(k))*std::exp(-.5*delta*delta/(width*width));
        }
        const float y=(float)(r.getHeight()*.5-(juce::jlimit)(-36.,36.,gain)*r.getHeight()/72.);
        if(x==0)curve.startNewSubPath((float)x,y);else curve.lineTo((float)x,y);
    }
    g.setColour(grass);g.strokePath(curve,juce::PathStrokeType(3.f));
    for(int k=0;k<SauceDSP::eqBands;++k){
        const auto p=point(k);g.setColour(black);g.fillRect((int)p.x-9,(int)p.y-9,18,18);
        g.setColour(k%2==0?gold:grass);g.fillRect((int)p.x-6,(int)p.y-6,12,12);
    }
    pixelText(g,"+36",{2,3,51,23},15,pale);
    pixelText(g,"0",{3,r.getHeight()/2-13,37,22},15,pale);
    pixelText(g,"-36",{2,r.getHeight()-51,51,23},15,pale);
    pixelText(g,"30 Hz",{10,r.getHeight()-24,79,22},13,pale);
    pixelText(g,"16.5 kHz",{r.getWidth()-106,r.getHeight()-24,103,22},13,pale);
    g.setColour(juce::Colour::fromRGB(82,111,80));g.drawRect(r,2);
}
void SauceEQGraph::mouseDown(const juce::MouseEvent& e){
    float nearest=1.e20f;selected=0;
    for(int i=0;i<SauceDSP::eqBands;++i){
        const float d=point(i).getDistanceSquaredFrom(e.position);
        if(d<nearest){nearest=d;selected=i;}
    }
    processor.parameters.getParameter(SauceDSP::ids[(size_t)eqGainId(selected)])->beginChangeGesture();
    processor.parameters.getParameter(SauceDSP::ids[(size_t)eqFreqId(selected)])->beginChangeGesture();
    movePoint(e);
}
void SauceEQGraph::mouseDrag(const juce::MouseEvent& e){if(selected>=0)movePoint(e);}
void SauceEQGraph::movePoint(const juce::MouseEvent& e){
    if(selected<0)return;
    const auto r=getLocalBounds().reduced(14,11).toFloat();
    const double xf=(juce::jlimit)(0.,1.,double((e.position.x-r.getX())/r.getWidth()));
    const double yf=(juce::jlimit)(0.,1.,double((e.position.y-r.getY())/r.getHeight()));
    const float hz=(float)(30.*std::pow(16500./30.,xf)),gain=(float)(36.-72.*yf);
    auto* f=processor.parameters.getParameter(SauceDSP::ids[(size_t)eqFreqId(selected)]);
    auto* g=processor.parameters.getParameter(SauceDSP::ids[(size_t)eqGainId(selected)]);
    f->setValueNotifyingHost(f->convertTo0to1(hz));
    g->setValueNotifyingHost(g->convertTo0to1(gain));
    repaint();
}
void SauceEQGraph::mouseUp(const juce::MouseEvent&){
    if(selected<0)return;
    processor.parameters.getParameter(SauceDSP::ids[(size_t)eqGainId(selected)])->endChangeGesture();
    processor.parameters.getParameter(SauceDSP::ids[(size_t)eqFreqId(selected)])->endChangeGesture();
    selected=-1;
}

VocalSauceEditor::VocalSauceEditor(VocalSauceProcessor& p)
 : juce::AudioProcessorEditor(&p),processor(p),graph(p) {
    background=juce::ImageCache::getFromMemory(BinaryData::reference_background_png,BinaryData::reference_background_pngSize);
    compressorCard=juce::ImageCache::getFromMemory(BinaryData::compressor_card_png,BinaryData::compressor_card_pngSize);
    setLookAndFeel(&look);
    setSize(W,H);
    for(int i=0;i<9;++i){
        auto& e=effects[(size_t)i];
        addAndMakeVisible(e.knob);addAndMakeVisible(e.expand);
        e.knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        e.knob.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        e.knob.setDoubleClickReturnValue(true,i==4?100.:0.);
        e.attachment=std::make_unique<Attachment>(processor.parameters,SauceDSP::ids[(size_t)i],e.knob);
        e.expand.onClick=[this,i]{setExpanded(i);};
        e.knob.onValueChange=[this]{repaint();};
    }
    addAndMakeVisible(graph);addAndMakeVisible(eqExpand);addAndMakeVisible(detailClose);
    eqExpand.onClick=[this]{setExpanded(9);};
    detailClose.onClick=[this]{setExpanded(-1);};
    for(int i=0;i<6;++i){
        auto& k=eqKnobs[(size_t)i];addAndMakeVisible(k);
        k.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        k.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        k.setDoubleClickReturnValue(true,0.);
        eqAttachments[(size_t)i]=std::make_unique<Attachment>(processor.parameters,SauceDSP::ids[(size_t)eqGainId(i)],k);
        k.onValueChange=[this]{repaint();};
    }
    for(size_t i=0;i<details.size();++i){
        auto& d=details[i];d.owner=specs[i].owner;
        addAndMakeVisible(d.slider);addAndMakeVisible(d.name);addAndMakeVisible(d.value);
        d.slider.setSliderStyle(juce::Slider::LinearHorizontal);
        d.slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        d.name.setText(specs[i].name,juce::dontSendNotification);
        d.name.setJustificationType(juce::Justification::centredLeft);
        d.name.setFont(px(13.f));d.name.setColour(juce::Label::textColourId,pale);
        d.value.setJustificationType(juce::Justification::centred);
        d.value.setFont(px(13.f));d.value.setColour(juce::Label::textColourId,gold);
        d.attachment=std::make_unique<Attachment>(processor.parameters,specs[i].id,d.slider);
        d.slider.onValueChange=[this,i]{
            auto& k=details[i];k.value.setText(juce::String(k.slider.getValue(),1),juce::dontSendNotification);
        };
        d.slider.onValueChange();
    }
    addAndMakeVisible(saveButton);addAndMakeVisible(loadButton);
    saveButton.onClick=[this]{savePreset();};loadButton.onClick=[this]{loadPreset();};
    resized();setExpanded(-1);startTimerHz(20);
}
VocalSauceEditor::~VocalSauceEditor(){stopTimer();chooser.reset();setLookAndFeel(nullptr);}

void VocalSauceEditor::paint(juce::Graphics& g){
    if(background.isValid())g.drawImageAt(background,0,0);
    else{g.fillAll(juce::Colour::fromRGB(68,48,35));pixelText(g,"17TESS VOCAL SAUCE",{30,30,900,80},35,pale);}
    for(int i=0;i<8;++i){
        const int x=tileX[i],y=tileY[i];
        g.setColour(black);g.fillRect(x+67,y+150,137,31);
        pixelText(g,juce::String(effects[(size_t)i].knob.getValue(),1)+" %",{x+67,y+149,137,31},20,gold);
    }
    if(expanded==-1){
        if(compressorCard.isValid())g.drawImageAt(compressorCard,compX,compY);
        pixelText(g,"COMPRESSOR",{compX+34,compY+10,203,34},20,pale);
        g.setColour(black);g.fillRect(compX+67,compY+150,137,31);
        pixelText(g,juce::String(effects[8].knob.getValue(),1)+" %",{compX+67,compY+149,137,31},20,gold);
        pixelText(g,"CLICK + FOR DETAILS",{806,914,352,27},16,pale);
    } else {
        // Keep the reference screenshot's decorative title bar and soil texture.
        g.setColour(juce::Colour::fromRGB(31,23,15).withAlpha(.75f));
        g.fillRect(813,704,338,33);
        pixelText(g,expanded==9?"EQ BAND WIDTH":mainNames[(size_t)expanded],{817,707,321,29},17,gold);
    }
    // 6 independently movable EQ bands instead of screenshot's original three.
    for(int i=0;i<6;++i){
        const int x=72+i*112;
        pixelText(g,"B"+juce::String(i+1),{x+2,921,76,21},13,pale);
        g.setColour(black);g.fillRect(x-4,940,88,23);
        pixelText(g,juce::String(eqKnobs[(size_t)i].getValue(),1)+" dB",{x-4,940,88,23},12,gold);
    }
    // Cover original screenshot version label, not the green NO FACTORY PRESETS indicators.
    g.setColour(juce::Colour::fromRGB(61,39,22));g.fillRect(243,1010,89,33);
    pixelText(g,"V0.4",{244,1014,85,23},14,pale);
}

void VocalSauceEditor::resized(){
    for(int i=0;i<9;++i){
        const int x=(i<8)?tileX[i]:compX;
        const int y=(i<8)?tileY[i]:compY;
        auto& e=effects[(size_t)i];
        e.knob.setBounds(x+80,y+47,110,102);
        e.expand.setBounds(x+229,y+11,41,41);
        e.knob.setVisible(i<8 || expanded==-1);
        e.expand.setVisible(i<8 || expanded==-1);
    }
    graph.setBounds(43,690,710,164);
    eqExpand.setBounds(715,641,44,46);
    detailClose.setBounds(1123,640,44,45);
    for(int i=0;i<6;++i)eqKnobs[(size_t)i].setBounds(77+i*112,865,66,66);
    int row=0;
    for(auto& d:details){
        const bool visible=d.owner==expanded;
        d.slider.setVisible(visible);d.name.setVisible(visible);d.value.setVisible(visible);
        if(visible){
            const int y=735+row*36;
            d.name.setBounds(807,y,134,26);
            d.slider.setBounds(939,y,154,26);
            d.value.setBounds(1091,y,67,26);
            ++row;
        }
    }
    saveButton.setBounds(746,982,215,70);
    loadButton.setBounds(973,982,217,70);
}
void VocalSauceEditor::setExpanded(int selected){
    expanded=(selected==expanded || selected<0)?-1:selected;
    resized();repaint();
}
void VocalSauceEditor::timerCallback(){graph.repaint();repaint();}
void VocalSauceEditor::showError(const juce::String& e){
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"17TESS VOCAL SAUCE",e);
}
void VocalSauceEditor::savePreset(){
    auto initial=processor.getPresetDirectory().getChildFile("my_vocal.17tesspreset");
    chooser=std::make_unique<juce::FileChooser>("Save 17TESS preset",initial,"*.17tesspreset");
    juce::Component::SafePointer<VocalSauceEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode|
            juce::FileBrowserComponent::canSelectFiles|
            juce::FileBrowserComponent::warnAboutOverwriting,
        [safe](const juce::FileChooser& fc){
            if(!safe)return;auto file=fc.getResult();if(file==juce::File())return;
            if(!file.hasFileExtension("17tesspreset"))file=file.withFileExtension(".17tesspreset");
            juce::String error;if(!safe->processor.savePreset(file,error))safe->showError(error);
        });
}
void VocalSauceEditor::loadPreset(){
    chooser=std::make_unique<juce::FileChooser>("Load 17TESS preset",processor.getPresetDirectory(),"*.17tesspreset");
    juce::Component::SafePointer<VocalSauceEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& fc){
            if(!safe)return;auto file=fc.getResult();if(!file.existsAsFile())return;
            juce::String error;if(!safe->processor.loadPreset(file,error))safe->showError(error);
        });
}

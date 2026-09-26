// Renders the editor to a PNG, no host needed.
//
//   the89th-snapshot out.png [width] [scene]
//
// Scenes: pitch (default), delay, freeze, quasi. Audio is run through the real
// processor first so the rings show a live state, stopping mid-splice where
// the scene has splices.

#include "../src/ParameterIDs.h"
#include "../src/PluginEditor.h"
#include "../src/PluginProcessor.h"

#include <cmath>
#include <cstdio>

namespace
{
void set (The89thProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void runAudio (The89thProcessor& p, int blocks, bool stopMidSplice)
{
    juce::AudioBuffer<float> b (2, 512);
    juce::MidiBuffer midi;
    static long long n = 0;

    for (int k = 0; k < blocks || (stopMidSplice && k < blocks + 400); ++k)
    {
        for (int i = 0; i < 512; ++i, ++n)
        {
            const double t = static_cast<double> (n) / 48000.0;
            float x = 0.0f;
            for (int h = 1; h <= 5; ++h)
                x += static_cast<float> (std::sin (2.0 * M_PI * 196.0 * h * t) / h);
            x *= 0.22f * static_cast<float> (0.6 + 0.4 * std::sin (2.0 * M_PI * 0.7 * t));
            b.setSample (0, i, x);
            b.setSample (1, i, x);
        }
        p.processBlock (b, midi);

        if (stopMidSplice && k >= blocks && p.telemetry().voice[0].splicing.load()
            && p.telemetry().voice[0].gainB.load() > 0.3f)
            break;
    }
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf (stderr, "usage: the89th-snapshot out.png [width] [pitch|delay|freeze|quasi]\n");
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI gui;

    const auto out   = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    const int  width = argc > 2 ? std::atoi (argv[2]) : The89thEditor::kBaseW;
    const juce::String scene = argc > 3 ? argv[3] : "pitch";

    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);

    bool splices = true;
    if (scene == "delay")
    {
        set (p, pid::mode, 0.0f);
        set (p, pid::channel[0].delay, 0.35f);
        set (p, pid::channel[1].delay, 0.62f);
        set (p, pid::channel[0].feedback, 0.45f);
        set (p, pid::channel[1].feedback, 0.3f);
        splices = false;
    }
    else
    {
        set (p, pid::channel[0].pitch, 1.5f);
        set (p, pid::channel[0].crosspoint1, 0.12f);
        set (p, pid::channel[0].crosspoint2, 0.58f);
        set (p, pid::channel[0].feedback, 0.4f);
        set (p, pid::channel[1].pitch, 0.75f);
        set (p, pid::channel[1].crosspoint1, 0.72f);   // deeper than crosspoint 2: reverse
        set (p, pid::channel[1].crosspoint2, 0.25f);
        set (p, pid::channel[1].vibratoDepth, 0.3f);

        if (scene == "quasi")
        {
            set (p, pid::stereo, 1.0f);
            set (p, pid::bandwidth, 2.0f);
        }
    }

    runAudio (p, 160, splices && scene != "freeze");

    if (scene == "freeze")
    {
        set (p, pid::freeze, 1.0f);
        runAudio (p, 60, true);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto* editor = dynamic_cast<The89thEditor*> (ed.get());
    editor->setSize (width, juce::roundToInt (width * static_cast<double> (The89thEditor::kBaseH) / The89thEditor::kBaseW));
    for (int i = 0; i < 3; ++i)
        editor->refresh();

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);

    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat png;
    if (! os.openedOk() || ! png.writeImageToStream (image, os))
    {
        std::fprintf (stderr, "the89th-snapshot: could not write %s\n", out.getFullPathName().toRawUTF8());
        return 1;
    }

    std::printf ("%s  %dx%d  scene %s\n", out.getFullPathName().toRawUTF8(),
                 editor->getWidth(), editor->getHeight(), scene.toRawUTF8());
    ed.reset();
    return 0;
}

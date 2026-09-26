// Renders the editor to a PNG, no host needed.
//
//   the89th-snapshot out.png [width] [scene]
//
// Scenes: pitch (default), delay, freeze, quasi, keys (the keyboard holding
// a fifth above the root), kb (the keyboard page, biphonic, with its
// sections on), modern (a factory preset
// using the modern controls, with Link on). Audio is run through the real
// processor first so the rings show a live state, stopping mid-splice where
// the scene has splices.

#include "../src/ParameterIDs.h"
#include "../src/PluginEditor.h"
#include "../src/PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
void set (The89thProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

std::vector<int> heldNotes;   // the keyboard scenes hold these keys down

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
        if (k == 0)
            for (int note : heldNotes)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, 1.0f), 0);
        p.processBlock (b, midi);
        midi.clear();

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
        std::fprintf (stderr, "usage: the89th-snapshot out.png [width] [pitch|delay|freeze|quasi|modern|keys|kb]\n");
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI gui;

    const auto out   = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    const int  width = argc > 2 ? std::atoi (argv[2]) : The89thEditor::kBaseW;
    const juce::String scene = argc > 3 ? argv[3] : "pitch";

    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);

    bool splices = true;
    if (scene == "keys")
    {
        set (p, pid::keys, 1.0f);
        set (p, pid::channel[0].crosspoint2, 0.4f);
        set (p, pid::channel[1].crosspoint2, 0.4f);
        heldNotes = { 67 };   // a fifth above the C3 root
        splices = false;
    }
    else if (scene == "kb")
    {
        set (p, pid::keys, 3.0f);             // biphonic
        set (p, pid::kbEnv, 1.0f);
        set (p, pid::kbAttack, 0.08f);
        set (p, pid::kbVib, 1.0f);
        set (p, pid::kbVibModDepth, 0.4f);
        set (p, pid::kbSynchro, 1.0f);        // left
        set (p, pid::kbAttackPt, 0.2f);
        set (p, pid::kbReturnPt, 0.4f);
        set (p, pid::kbEndPt, 0.8f);
        set (p, pid::kbReverse, 2.0f);        // right
        set (p, pid::channel[0].crosspoint2, 0.1f);
        set (p, pid::channel[1].crosspoint1, 0.3f);
        set (p, pid::channel[1].crosspoint2, 0.05f);
        heldNotes = { 55, 64 };
        splices = false;
    }
    else if (scene == "modern")
    {
        const auto& list = p.presets.entries();
        for (int i = 0; i < static_cast<int> (list.size()); ++i)
            if (list[static_cast<std::size_t> (i)].name == "Ping-Pong Eighths")
                p.presets.load (i);
        set (p, pid::snap, 1.0f);
        set (p, pid::link, 1.0f);
        set (p, pid::scrubDepth, 0.4f);
        p.presets.selectSlot (1);
        splices = false;
    }
    else if (scene == "delay")
    {
        set (p, pid::channel[0].mode, 0.0f);
        set (p, pid::channel[1].mode, 0.0f);
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
        set (p, pid::channel[0].freeze, 1.0f);
        set (p, pid::channel[1].freeze, 1.0f);
        runAudio (p, 60, true);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto* editor = dynamic_cast<The89thEditor*> (ed.get());
    editor->setSize (width, juce::roundToInt (width * static_cast<double> (The89thEditor::kBaseH) / The89thEditor::kBaseW));
    if (scene == "kb")
        editor->showKeyboardPage (true);
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

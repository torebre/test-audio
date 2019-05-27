#include <thread>
#include "ProcessAudioInput.h"


MainContentComponent::MainContentComponent() {
    essentia::init();

    addAndMakeVisible(start);
    start.setButtonText("Start");
    start.onClick = [this] { startClicked(); };

    addAndMakeVisible(saveSample);
    saveSample.setButtonText("Save sample");
    saveSample.onClick = [this] { saveClicked(); };

    setSize(600, 100);
    setAudioChannels(2, 2);

}


void MainContentComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    if(!networkSetup) {
        setupNetwork(samplesPerBlockExpected, sampleRate);
        networkSetup = true;
    }
}

void MainContentComponent::setupNetwork(int samplesPerBlockExpected, double sampleRate) {
    DBG("Samples per block expected: " + std::to_string(samplesPerBlockExpected) + ". Sample rate: " +
        std::to_string(sampleRate));

    AlgorithmFactory &factory = streaming::AlgorithmFactory::instance();

//    gen = static_cast<RingBufferInput*>(streaming::AlgorithmFactory::create("RingBufferInput", "bufferSize", frameSize));

    gen = new RingBufferInput();
    gen->declareParameters();
    gen->configure();

//    gen->setParameters(parameterMap);
//        gen->_bufferSize = frameSize;

    gen->output(0).setAcquireSize(frameSize);
    gen->output(0).setReleaseSize(frameSize);

    Algorithm *fc = factory.create("FrameCutter",
                                   "frameSize", frameSize,
                                   "hopSize", hopSize);

    Algorithm *w = factory.create("Windowing",
                                  "type", "hann");
    Algorithm *spec = factory.create("Spectrum");

    gen->output("signal") >> fc->input("signal");

    fc->output("frame") >> w->input("frame");
    w->output("frame") >> spec->input("frame");
    streaming::Algorithm *pitchYinFft = streaming::AlgorithmFactory::create("PitchYinFFT",
                                                                            "frameSize", frameSize);
    spec->output("spectrum") >> pitchYinFft->input("spectrum");
    pitchYinFft->output("pitch") >> essentia::streaming::PoolConnector(pool, "pitch");
    pitchYinFft->output("pitchConfidence") >> essentia::streaming::PoolConnector(pool, "pitch_confidence");

//    spec->output("spectrum") >> essentia::streaming::PoolConnector(pool, "spectrum");


    streaming::Algorithm *pitchMelodia = streaming::AlgorithmFactory::create("PitchMelodia", "frameSize", frameSize,
                                                                             "hopSize", hopSize, "guessUnvoiced", true);
    gen->output("signal") >> pitchMelodia->input("signal");

    pitchMelodia->output("pitch") >> essentia::streaming::PoolConnector(pool, "melodia_pitch");
    pitchMelodia->output("pitchConfidence") >> essentia::streaming::PoolConnector(pool, "melodia_pitch_confidence");

    network = new scheduler::Network(gen);
//        network->initStack();


// TODO Is it necessary to run this here?
    audioProcessor = std::thread(&MainContentComponent::runNetwork, this);
//    network->run();
}

void MainContentComponent::runNetwork() {
    network->run();
}


void MainContentComponent::getNextAudioBlock(const AudioSourceChannelInfo &bufferToFill) {
    if(!recording) {
        bufferToFill.clearActiveBufferRegion();
        return;
    }
    auto *device = deviceManager.getCurrentAudioDevice();
    auto activeInputChannels = device->getActiveInputChannels();
    auto activeOutputChannels = device->getActiveOutputChannels();
    auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
    auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;

    for (auto channel = 0;
         channel < maxOutputChannels;
         ++channel) {
        if ((!activeOutputChannels[channel]) || maxInputChannels == 0) {
            bufferToFill.buffer->
                    clear(channel, bufferToFill
                    .startSample, bufferToFill.numSamples);
        } else {
            auto actualInputChannel = channel % maxInputChannels;

            if (!activeInputChannels[channel]) {
                bufferToFill.buffer->
                        clear(channel, bufferToFill
                        .startSample, bufferToFill.numSamples);
            } else {
                auto *inBuffer = bufferToFill.buffer->getReadPointer(actualInputChannel,
                                                                     bufferToFill.startSample);
                auto *outBuffer = bufferToFill.buffer->getWritePointer(channel, bufferToFill.startSample);


                auto audioBuffer = std::vector<Real>(static_cast<unsigned long>(bufferToFill.numSamples));


                // TODO Is this needed?
//                if (audioBuffer.size() != frameSize) {
//                    gen->output(0).
//                            setAcquireSize(bufferToFill
//                                                   .numSamples);
//                    gen->output(0).
//                            setReleaseSize(bufferToFill
//                                                   .numSamples);
//
//                }

                for (
                        auto sample = 0;
                        sample < bufferToFill.
                                numSamples;
                        ++sample) {
                    outBuffer[sample] = inBuffer[sample];
                    audioBuffer[sample] = inBuffer[sample];
                }

                gen->add(&audioBuffer[0], bufferToFill.numSamples);


            }
        }
    }
}

void MainContentComponent::startClicked() {
    recording = true;
}

void MainContentComponent::saveClicked() {
    recording = false;
    FileBrowserComponent fileBrowserComponent(FileBrowserComponent::FileChooserFlags::saveMode,
                                              File(),
                                              nullptr,
                                              nullptr);
    FileChooserDialogBox fileChooserDialogBox("Save file", "Save output file from algorithm network",
                                              fileBrowserComponent, true, Colours::lightgrey);

    if (fileChooserDialogBox.show()) {

        // TODO

//    standard::Algorithm* pitchYinFft = standard::AlgorithmFactory::create("PitchYinFFT",
//            "frameSize", frameSize);

//    pitchYinFft->input()

        auto selectedFile = fileBrowserComponent.getSelectedFile(0);

        standard::Algorithm *output = standard::AlgorithmFactory::create("YamlOutput",
                                                                         "filename",
                                                                         selectedFile.getFullPathName().toStdString());


        auto descriptorNames = pool.descriptorNames();
        for (int i = 0; i < descriptorNames.size(); ++i) {
            DBG("Descriptor: " + descriptorNames[i]);
        }

        output->input("pool").set(pool);
        output->compute();

        networkSetup = false;



    }


}


void MainContentComponent::resized() {
    start.setBounds(10, 10, 90, 20);
    saveSample.setBounds(10, 40, 90, 20);
}
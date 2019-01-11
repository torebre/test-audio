#include <thread>
#include "ProcessAudioInput.h"


MainContentComponent::MainContentComponent() {
    essentia::init();

    levelSlider.setRange(0.0, 0.25);
    levelSlider.setTextBoxStyle(Slider::TextBoxRight, false, 100, 20);
    levelLabel.setText("Noise Level", dontSendNotification);

    addAndMakeVisible(start);
    start.setButtonText("Start");

    addAndMakeVisible(levelSlider);
    addAndMakeVisible(levelLabel);

    addAndMakeVisible(saveSample);
    saveSample.setButtonText("Save sample");
    saveSample.onClick = [this] { buttonClicked(); };

    setSize(600, 100);
    setAudioChannels(2, 2);

}


void MainContentComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
//    essentia::init();

    AlgorithmFactory &factory = streaming::AlgorithmFactory::instance();

//    gen = static_cast<RingBufferInput*>(streaming::AlgorithmFactory::create("RingBufferInput", "bufferSize", frameSize));

    gen = new RingBufferInput();
    gen->declareParameters();
    gen->configure();

//    gen->setParameters(parameterMap);
//        gen->_bufferSize = frameSize;
    gen->output(0).setAcquireSize(frameSize);
    gen->output(0).setReleaseSize(frameSize);

//    factory.create("RingBufferInput");

//        *gen = factory.create("RingBufferInput",
//                                          "bufferSize", frameSize);

    Algorithm *fc = factory.create("FrameCutter",
                                   "frameSize", frameSize,
                                   "hopSize", hopSize);

    Algorithm *w = factory.create("Windowing",
                                  "type", "blackmanharris62");

    Algorithm *spec = factory.create("Spectrum");
    Algorithm *mfcc = factory.create("MFCC");

/////////// CONNECTING THE ALGORITHMS ////////////////
    std::cout << "-------- connecting algos --------" << std::endl;

// Audio -> FrameCutter


    gen->output("signal") >> fc->input("signal");
//        audio->output("audio") >> fc->input("signal");

// FrameCutter -> Windowing -> Spectrum
    fc->output("frame") >> w->input("frame");
    w->output("frame") >> spec->input("frame");

// Spectrum -> MFCC -> Pool
//    spec->output("spectrum") >> mfcc->input("spectrum");

//    mfcc->output("bands") >> NOWHERE;                   // we don't want the mel bands
//    mfcc->output("mfcc") >> essentia::streaming::PoolConnector(pool, "lowlevel.mfcc"); // store only the mfcc coeffs


//    spec->output("spectrum") >> essentia::streaming::PoolConnector(pool, "spectrum");

    streaming::Algorithm* pitchYinFft = streaming::AlgorithmFactory::create("PitchYinFFT",
                                                                          "frameSize", frameSize);

    spec->output("spectrum") >> pitchYinFft->input("spectrum");

//    Algorithm *file = new FileOutput<Real>();
//    file->configure("filename", "test_output2.txt", "pitchyinfft", "text");

//    gen->output("signal") >> file->input("data");

    pitchYinFft->output("pitch") >> essentia::streaming::PoolConnector(pool, "pitch");
    pitchYinFft->output("pitchConfidence") >> essentia::streaming::PoolConnector(pool, "pitchConfidence");

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
    auto *device = deviceManager.getCurrentAudioDevice();
    auto activeInputChannels = device->getActiveInputChannels();
    auto activeOutputChannels = device->getActiveOutputChannels();
    auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
    auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;


    auto level = (float) levelSlider.getValue();

    for (
            auto channel = 0;
            channel < maxOutputChannels;
            ++channel) {
        if ((!activeOutputChannels[channel]) || maxInputChannels == 0) {
            bufferToFill.buffer->
                    clear(channel, bufferToFill
                    .startSample, bufferToFill.numSamples);
        } else {
            auto actualInputChannel = channel % maxInputChannels; // [1]

            if (!activeInputChannels[channel]) // [2]
            {
                bufferToFill.buffer->
                        clear(channel, bufferToFill
                        .startSample, bufferToFill.numSamples);
            } else // [3]
            {
                auto *inBuffer = bufferToFill.buffer->getReadPointer(actualInputChannel,
                                                                     bufferToFill.startSample);
                auto *outBuffer = bufferToFill.buffer->getWritePointer(channel, bufferToFill.startSample);


                auto audioBuffer = std::vector<Real>(static_cast<unsigned long>(bufferToFill.numSamples));


                if (audioBuffer.size() != frameSize) {
                    gen->output(0).
                            setAcquireSize(bufferToFill
                                                   .numSamples);
                    gen->output(0).
                            setReleaseSize(bufferToFill
                                                   .numSamples);

                }

                for (
                        auto sample = 0;
                        sample < bufferToFill.
                                numSamples;
                        ++sample) {
                    outBuffer[sample] = inBuffer[sample];
                    audioBuffer[sample] = inBuffer[sample];
                }

//                std::cout << "Number of samples: "; // << &audioBuffer[0];

//                const std::string valueString("mfcc");
//                auto value = pool.value(&valueString);

//                auto temp = std::vector<Real> { 1.0, 2.0, 3.0, 4.0, 5.0};

//                for(unsigned long i = 0; i < temp.size(); ++i) {
//                    std::cout << "Test32: " << temp[i];
//                }


                gen->add(&audioBuffer[0], bufferToFill.numSamples);




//                auto keys = gen->parameterDescription.keys();
//                for(unsigned long i = 0; i < keys.size(); ++i) {
//                    std::cout << "Test31: " << gen->parameterDescription.keys()[i];
//                }
//                gen->add(&temp[0], 3);
            }
        }
    }
}

void MainContentComponent::buttonClicked() {
    DBG("Clicked");

    // TODO

//    standard::Algorithm* pitchYinFft = standard::AlgorithmFactory::create("PitchYinFFT",
//            "frameSize", frameSize);

//    pitchYinFft->input()


    standard::Algorithm* output = standard::AlgorithmFactory::create("YamlOutput",
                                                                     "filename", "test_pitchyinfft.yaml");
    output->input("pool").set(pool);
    output->compute();

}


void MainContentComponent::resized() {
    levelLabel.setBounds(10, 10, 90, 20);
    levelSlider.setBounds(100, 10, getWidth() - 110, 20);
    start.setBounds(10, 40, 90, 20);
    saveSample.setBounds(10, 40, 90, 20);
}
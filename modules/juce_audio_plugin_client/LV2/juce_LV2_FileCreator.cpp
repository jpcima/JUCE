/*
  ==============================================================================

   Juce LV2 Wrapper

  ==============================================================================
*/

#if JucePlugin_Build_LV2_FileCreator

#include <cctype>

/** Plugin requires processing with a fixed/constant block size */
#ifndef JucePlugin_WantsLV2FixedBlockSize
#define JucePlugin_WantsLV2FixedBlockSize 0
#endif

/** Export presets */
#ifndef JucePlugin_WantsLV2Presets
#define JucePlugin_WantsLV2Presets 1
#endif


//==============================================================================
/**
 */
class JuceLV2FileCreator
{
    /** Returns plugin type, defined in AppConfig.h or JucePluginCharacteristics.h */
    static String getPluginType()
    {
        String pluginType;
#ifdef JucePlugin_LV2Category
        pluginType  = "lv2:" JucePlugin_LV2Category;
        pluginType += ", ";
#elif JucePlugin_IsSynth
        pluginType  = "lv2:InstrumentPlugin, ";
#endif
        pluginType += "lv2:Plugin";
        return pluginType;
    }

    /** Returns plugin extension */
    static String getPluginExtension()
    {
#if JUCE_MAC
        String pluginExtension(".dylib");
#elif JUCE_LINUX
        String pluginExtension(".so");
#elif JUCE_WINDOWS
        String pluginExtension(".dll");
#endif
        return pluginExtension;
    }

    /** Returns plugin URI */
    static String getPluginURI()
    {
        // JucePlugin_LV2URI might be defined as a function (eg. allowing dynamic URIs based on filename)
        return String(JucePlugin_LV2URI);
    }

    /** Queries all available plugin audio ports */
    static void findMaxTotalChannels (AudioProcessor* const filter, int& maxTotalIns, int& maxTotalOuts)
    {
        filter->enableAllBuses();

#ifdef JucePlugin_PreferredChannelConfigurations
        int configs[][2] = { JucePlugin_PreferredChannelConfigurations };
        maxTotalIns = maxTotalOuts = 0;

        for (auto& config : configs)
        {
            maxTotalIns =  jmax (maxTotalIns,  config[0]);
            maxTotalOuts = jmax (maxTotalOuts, config[1]);
        }
#else
        auto numInputBuses  = filter->getBusCount (true);
        auto numOutputBuses = filter->getBusCount (false);

        if (numInputBuses > 1 || numOutputBuses > 1)
        {
            maxTotalIns = maxTotalOuts = 0;

            for (int i = 0; i < numInputBuses; ++i)
                maxTotalIns  += filter->getChannelCountOfBus (true, i);

            for (int i = 0; i < numOutputBuses; ++i)
                maxTotalOuts += filter->getChannelCountOfBus (false, i);
        }
        else
        {
            maxTotalIns  = numInputBuses  > 0 ? filter->getBus (true,  0)->getMaxSupportedChannels (64) : 0;
            maxTotalOuts = numOutputBuses > 0 ? filter->getBus (false, 0)->getMaxSupportedChannels (64) : 0;
        }
#endif
    }

    static Array<String>& getUsedSymbols()
    {
        static Array<String> usedSymbols;
        return usedSymbols;
    }

    /** Converts a parameter name to an LV2 compatible symbol. */
    static String nameToSymbol (const String& name, const uint32 portIndex)
    {
        String symbol, trimmedName = name.trimStart().trimEnd().toLowerCase();

        if (trimmedName.isEmpty())
        {
            symbol += "lv2_port_";
            symbol += String(portIndex+1);
        }
        else
        {
            for (int i=0; i < trimmedName.length(); ++i)
            {
                const juce_wchar c = trimmedName[i];
                if (i == 0 && std::isdigit(c))
                    symbol += "_";
                else if (std::isalpha(c) || std::isdigit(c))
                    symbol += c;
                else
                    symbol += "_";
            }
        }

        // Do not allow identical symbols
        if (getUsedSymbols().contains(symbol))
        {
            int offset = 2;
            String offsetStr = "_2";
            symbol += offsetStr;

            while (getUsedSymbols().contains(symbol))
            {
                offset += 1;
                String newOffsetStr = "_" + String(offset);
                symbol = symbol.replace(offsetStr, newOffsetStr);
                offsetStr = newOffsetStr;
            }
        }
        getUsedSymbols().add(symbol);

        return symbol;
    }

    /** Create the manifest.ttl file contents */
    static String makeManifestFile (AudioProcessor* const filter, const String& binary)
    {
        String pluginURI(getPluginURI());
        String text;

        // Header
        text += "@prefix lv2:  <" LV2_CORE_PREFIX "> .\n";
        text += "@prefix pset: <" LV2_PRESETS_PREFIX "> .\n";
        text += "@prefix rdf:   <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .\n";
        text += "@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .\n";
        text += "@prefix ui:   <" LV2_UI_PREFIX "> .\n";
        text += "\n";

        // Plugin
        text += "<" + pluginURI + ">\n";
        text += "    a lv2:Plugin ;\n";
        text += "    lv2:binary <" + binary + getPluginExtension() + "> ;\n";
        text += "    rdfs:seeAlso <" + binary + ".ttl> .\n";
        text += "\n";

#if ! JUCE_AUDIOPROCESSOR_NO_GUI
        // UIs
        if (filter->hasEditor())
        {
            text += "<" + pluginURI + "#ExternalUI>\n";
            text += "    a <" LV2_EXTERNAL_UI__Widget "> ;\n";
            text += "    ui:binary <" + binary + getPluginExtension() + "> ;\n";
            text += "    lv2:requiredFeature <" LV2_INSTANCE_ACCESS_URI "> ;\n";
            text += "    lv2:extensionData <" LV2_PROGRAMS__UIInterface "> .\n";
            text += "\n";

            text += "<" + pluginURI + "#ParentUI>\n";
#if JUCE_MAC
            text += "    a ui:CocoaUI ;\n";
#elif JUCE_LINUX
            text += "    a ui:X11UI ;\n";
#elif JUCE_WINDOWS
            text += "    a ui:WindowsUI ;\n";
#endif
            text += "    ui:binary <" + binary + getPluginExtension() + "> ;\n";
            text += "    lv2:requiredFeature <" LV2_INSTANCE_ACCESS_URI "> ;\n";
            text += "    lv2:optionalFeature ui:noUserResize ;\n";
            text += "    lv2:extensionData <" LV2_PROGRAMS__UIInterface "> .\n";
            text += "\n";
        }
#endif

#if JucePlugin_WantsLV2Presets
        const String presetSeparator(pluginURI.contains("#") ? ":" : "#");

        // Presets
        for (int i = 0; i < filter->getNumPrograms(); ++i)
        {
            text += "<" + pluginURI + presetSeparator + "preset" + String::formatted("%03i", i+1) + ">\n";
            text += "    a pset:Preset ;\n";
            text += "    lv2:appliesTo <" + pluginURI + "> ;\n";
            text += "    rdfs:label \"" + filter->getProgramName(i) + "\" ;\n";
            text += "    rdfs:seeAlso <presets.ttl> .\n";
            text += "\n";
        }
#endif

        return text;
    }

    enum
    {
        parameterNameMaxLength = 256
    };

    /** Create the -plugin-.ttl file contents */
    static String makePluginFile (AudioProcessor* const filter, const int maxNumInputChannels, const int maxNumOutputChannels)
    {
        String pluginURI(getPluginURI());
        String text;

        // Header
        text += "@prefix atom: <" LV2_ATOM_PREFIX "> .\n";
        text += "@prefix doap: <http://usefulinc.com/ns/doap#> .\n";
        text += "@prefix foaf: <http://xmlns.com/foaf/0.1/> .\n";
        text += "@prefix lv2:  <" LV2_CORE_PREFIX "> .\n";
        text += "@prefix rdf:   <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .\n";
        text += "@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .\n";
        text += "@prefix ui:   <" LV2_UI_PREFIX "> .\n";
        text += "\n";

        // Plugin
        text += "<" + pluginURI + ">\n";
        text += "    a " + getPluginType() + " ;\n";
        text += "    lv2:requiredFeature <" LV2_BUF_SIZE__boundedBlockLength "> ,\n";
#if JucePlugin_WantsLV2FixedBlockSize
        text += "                        <" LV2_BUF_SIZE__fixedBlockLength "> ,\n";
#endif
        text += "                        <" LV2_URID__map "> ;\n";
        text += "    lv2:extensionData <" LV2_OPTIONS__interface "> ,\n";
#if JucePlugin_WantsLV2State
        text += "                      <" LV2_STATE__interface "> ,\n";
#endif
        text += "                      <" LV2_PROGRAMS__Interface "> ;\n";
        text += "\n";

#if ! JUCE_AUDIOPROCESSOR_NO_GUI
        // UIs
        if (filter->hasEditor())
        {
            text += "    ui:ui <" + pluginURI + "#ExternalUI> ,\n";
            text += "          <" + pluginURI + "#ParentUI> ;\n";
            text += "\n";
        }
#endif

        uint32 portIndex = 0;

#if (JucePlugin_WantsMidiInput || JucePlugin_WantsLV2TimePos)
        // MIDI input
        text += "    lv2:port [\n";
        text += "        a lv2:InputPort, atom:AtomPort ;\n";
        text += "        atom:bufferType atom:Sequence ;\n";
#if JucePlugin_WantsMidiInput
        text += "        atom:supports <" LV2_MIDI__MidiEvent "> ;\n";
#endif
#if JucePlugin_WantsLV2TimePos
        text += "        atom:supports <" LV2_TIME__Position "> ;\n";
#endif
        text += "        lv2:index " + String(portIndex++) + " ;\n";
        text += "        lv2:symbol \"lv2_events_in\" ;\n";
        text += "        lv2:name \"Events Input\" ;\n";
        text += "        lv2:designation lv2:control ;\n";
#if ! JucePlugin_IsSynth
        text += "        lv2:portProperty lv2:connectionOptional ;\n";
#endif
        text += "    ] ;\n";
        text += "\n";
#endif

#if JucePlugin_ProducesMidiOutput
        // MIDI output
        text += "    lv2:port [\n";
        text += "        a lv2:OutputPort, atom:AtomPort ;\n";
        text += "        atom:bufferType atom:Sequence ;\n";
        text += "        atom:supports <" LV2_MIDI__MidiEvent "> ;\n";
        text += "        lv2:index " + String(portIndex++) + " ;\n";
        text += "        lv2:symbol \"lv2_midi_out\" ;\n";
        text += "        lv2:name \"MIDI Output\" ;\n";
        text += "    ] ;\n";
        text += "\n";
#endif

        // Freewheel port
        text += "    lv2:port [\n";
        text += "        a lv2:InputPort, lv2:ControlPort ;\n";
        text += "        lv2:index " + String(portIndex++) + " ;\n";
        text += "        lv2:symbol \"lv2_freewheel\" ;\n";
        text += "        lv2:name \"Freewheel\" ;\n";
        text += "        lv2:default 0.0 ;\n";
        text += "        lv2:minimum 0.0 ;\n";
        text += "        lv2:maximum 1.0 ;\n";
        text += "        lv2:designation <" LV2_CORE__freeWheeling "> ;\n";
        text += "        lv2:portProperty lv2:toggled, <" LV2_PORT_PROPS__notOnGUI "> ;\n";
        text += "    ] ;\n";
        text += "\n";

#if JucePlugin_WantsLV2Latency
        // Latency port
        text += "    lv2:port [\n";
        text += "        a lv2:OutputPort, lv2:ControlPort ;\n";
        text += "        lv2:index " + String(portIndex++) + " ;\n";
        text += "        lv2:symbol \"lv2_latency\" ;\n";
        text += "        lv2:name \"Latency\" ;\n";
        text += "        lv2:designation <" LV2_CORE__latency "> ;\n";
        text += "        lv2:portProperty lv2:reportsLatency, lv2:integer ;\n";
        text += "    ] ;\n";
        text += "\n";
#endif

        // Audio inputs
        for (int i=0; i < maxNumInputChannels; ++i)
        {
            if (i == 0)
                text += "    lv2:port [\n";
            else
                text += "    [\n";

            text += "        a lv2:InputPort, lv2:AudioPort ;\n";
            text += "        lv2:index " + String(portIndex++) + " ;\n";
            text += "        lv2:symbol \"lv2_audio_in_" + String(i+1) + "\" ;\n";
            text += "        lv2:name \"Audio Input " + String(i+1) + "\" ;\n";

            if (i+1 == maxNumInputChannels)
                text += "    ] ;\n\n";
            else
                text += "    ] ,\n";
        }

        // Audio outputs
        for (int i=0; i < maxNumOutputChannels; ++i)
        {
            if (i == 0)
                text += "    lv2:port [\n";
            else
                text += "    [\n";

            text += "        a lv2:OutputPort, lv2:AudioPort ;\n";
            text += "        lv2:index " + String(portIndex++) + " ;\n";
            text += "        lv2:symbol \"lv2_audio_out_" + String(i+1) + "\" ;\n";
            text += "        lv2:name \"Audio Output " + String(i+1) + "\" ;\n";

            if (i+1 == maxNumOutputChannels)
                text += "    ] ;\n\n";
            else
                text += "    ] ,\n";
        }

        // Parameters
        OwnedArray<Lv2ParameterWrapper> parameters = wrapParameters (filter);
        const OwnedArray<AudioProcessorParameter> &rawParams = filter->getParameters();
        for (int i=0; i < parameters.size(); ++i)
        {
            if (i == 0)
                text += "    lv2:port [\n";
            else
                text += "    [\n";

            String paramID;
            if (AudioProcessorParameterWithID *p = dynamic_cast<AudioProcessorParameterWithID *>(rawParams[i]))
                paramID = p->paramID;
            else
                paramID = "port" + String(i+1);

            text += "        a lv2:InputPort, lv2:ControlPort ;\n";
            text += "        lv2:index " + String(portIndex++) + " ;\n";
            text += "        lv2:symbol \"" + nameToSymbol(paramID, i) + "\" ;\n";

            String const paramName = rawParams[i]->getName(parameterNameMaxLength);
            if (paramName.isNotEmpty())
                text += "        lv2:name \"" + paramName + "\" ;\n";
            else
                text += "        lv2:name \"Port " + String(i+1) + "\" ;\n";

            float minValue = parameters[i]->getMinimum();
            float maxValue = parameters[i]->getMaximum();
            float defValue = parameters[i]->getDefault();

            text += "        lv2:default " + String(defValue) + " ;\n";
            text += "        lv2:minimum " + String(minValue) + " ;\n";
            text += "        lv2:maximum " + String(maxValue) + " ;\n";

            switch (parameters[i]->type())
            {
            default:
                break;
            case Lv2ParameterInt:
                text += "        lv2:portProperty lv2:integer ;\n";
                break;
            case Lv2ParameterBool:
                text += "        lv2:portProperty lv2:toggled ;\n";
                break;
            case Lv2ParameterChoice:
            {
                text += "        lv2:portProperty lv2:integer, lv2:enumeration ;\n";
                Lv2ChoiceParameterWrapper *w = static_cast<Lv2ChoiceParameterWrapper *>(parameters[i]);
                const StringArray &choices = w->parameter->choices;
                for (int i=0; i < choices.size(); i++)
                    text += "        lv2:scalePoint [ rdfs:label \"" + choices[i] + "\" ; rdf:value " + String(i) + " ] ;\n";
                break;
            }
            }

            if (! rawParams[i]->isAutomatable())
                text += "        lv2:portProperty <" LV2_PORT_PROPS__expensive "> ;\n";

            if (i+1 == parameters.size())
                text += "    ] ;\n\n";
            else
                text += "    ] ,\n";
        }

        text += "    doap:name \"" + filter->getName() + "\" ;\n";
        text += "    doap:maintainer [ foaf:name \"" JucePlugin_Manufacturer "\" ] .\n";

        return text;
    }

    /** Create the presets.ttl file contents */
    static String makePresetsFile (AudioProcessor* const filter)
    {
        String pluginURI(getPluginURI());
        String text;

        // Header
        text += "@prefix atom:  <" LV2_ATOM_PREFIX "> .\n";
        text += "@prefix lv2:   <" LV2_CORE_PREFIX "> .\n";
        text += "@prefix pset:  <" LV2_PRESETS_PREFIX "> .\n";
        text += "@prefix rdf:   <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .\n";
        text += "@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#> .\n";
        text += "@prefix state: <" LV2_STATE_PREFIX "> .\n";
        text += "@prefix xsd:   <http://www.w3.org/2001/XMLSchema#> .\n";
        text += "\n";

        // Presets
        const int numPrograms = filter->getNumPrograms();
        const String presetSeparator(pluginURI.contains("#") ? ":" : "#");
        OwnedArray<Lv2ParameterWrapper> parameters = wrapParameters (filter);

        for (int i = 0; i < numPrograms; ++i)
        {
            std::cout << "\nSaving preset " << i+1 << "/" << numPrograms+1 << "...";
            std::cout.flush();

            String preset;

            // Label
            filter->setCurrentProgram(i);
            preset += "<" + pluginURI + presetSeparator + "preset" + String::formatted("%03i", i+1) + "> a pset:Preset ;\n";

            // State
            const OwnedArray<AudioProcessorParameter>& rawParams = filter->getParameters();
#if JucePlugin_WantsLV2State
            preset += "    state:state [\n";
#if JucePlugin_WantsLV2StateString
            preset += "        <" JUCE_LV2_STATE_STRING_URI ">\n";
            preset += "\"\"\"\n";
            preset += filter->getStateInformationString().replace("\r\n","\n");
            preset += "\"\"\"\n";
#else
            MemoryBlock chunkMemory;
            filter->getCurrentProgramStateInformation(chunkMemory);
            const String chunkString(Base64::toBase64(chunkMemory.getData(), chunkMemory.getSize()));

            preset += "        <" JUCE_LV2_STATE_BINARY_URI "> [\n";
            preset += "            a atom:Chunk ;\n";
            preset += "            rdf:value \"" + chunkString + "\"^^xsd:base64Binary ;\n";
            preset += "        ] ;\n";
#endif
            if (rawParams.size() == 0)
            {
                preset += "    ] .\n\n";
                continue;
            }

            preset += "    ] ;\n\n";
#endif

            // Port values
            getUsedSymbols().clear();
            for (int j=0; j < rawParams.size(); ++j)
            {
                  if (j == 0)
                    preset += "    lv2:port [\n";
                else
                    preset += "    [\n";

                  String paramID;
                  if (AudioProcessorParameterWithID *p = dynamic_cast<AudioProcessorParameterWithID *>(rawParams[j]))
                      paramID = p->paramID;
                  else
                      paramID = "port" + String(j+1);

                preset += "        lv2:symbol \"" + nameToSymbol(paramID, j) + "\" ;\n";
                float value = parameters[j]->getValue();
                preset += "        pset:value " + String(value) + " ;\n";

                if (j+1 == rawParams.size())
                    preset += "    ] ";
                else
                    preset += "    ] ,\n";
            }
            preset += ".\n\n";

            text += preset;
        }

        return text;
    }

public:
    /** Creates manifest.ttl, plugin.ttl and presets.ttl files */
    static void createLv2Files(const char* basename)
    {
        const ScopedJuceInitialiser_GUI juceInitialiser;
        ScopedPointer<AudioProcessor> filter(createPluginFilterOfType (AudioProcessor::wrapperType_LV2));

        int maxNumInputChannels, maxNumOutputChannels;
        findMaxTotalChannels(filter, maxNumInputChannels, maxNumOutputChannels);

        String binary(basename);
        String binaryTTL(binary + ".ttl");

        std::cout << "Writing manifest.ttl..."; std::cout.flush();
        std::fstream manifest("manifest.ttl", std::ios::out);
        manifest << makeManifestFile(filter, binary) << std::endl;
        manifest.close();
        std::cout << " done!" << std::endl;

        std::cout << "Writing " << binary << ".ttl..."; std::cout.flush();
        std::fstream plugin(binaryTTL.toUTF8(), std::ios::out);
        plugin << makePluginFile(filter, maxNumInputChannels, maxNumOutputChannels) << std::endl;
        plugin.close();
        std::cout << " done!" << std::endl;

#if JucePlugin_WantsLV2Presets
        std::cout << "Writing presets.ttl..."; std::cout.flush();
        std::fstream presets("presets.ttl", std::ios::out);
        presets << makePresetsFile(filter) << std::endl;
        presets.close();
        std::cout << " done!" << std::endl;
#endif
    }
};

#if JUCE_WINDOWS
 #define JUCE_EXPORTED_FUNCTION extern "C" __declspec (dllexport)
#else
 #define JUCE_EXPORTED_FUNCTION extern "C" __attribute__ ((visibility("default")))
#endif

//==============================================================================
// startup code..

JUCE_EXPORTED_FUNCTION void lv2_generate_ttl (const char* basename);
JUCE_EXPORTED_FUNCTION void lv2_generate_ttl (const char* basename)
{
    JuceLV2FileCreator::createLv2Files (basename);
}

#endif

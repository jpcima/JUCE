/*
  ==============================================================================

   Juce LV2 Parameter Wrapper

  ==============================================================================
*/

enum Lv2ParameterType
{
    Lv2ParameterFloat,
    Lv2ParameterInt,
    Lv2ParameterBool,
    Lv2ParameterChoice,
    Lv2ParameterGeneric
};

class Lv2ParameterWrapper
{
public:
    virtual ~Lv2ParameterWrapper() {}
    virtual Lv2ParameterType type() const = 0;
    virtual float getValue() const = 0;
    virtual void setValue(float value) const = 0;
    virtual float getDefault() const = 0;
    virtual float getMinimum() const = 0;
    virtual float getMaximum() const = 0;
    virtual float convertFrom0to1(float value) const = 0;
};

class Lv2FloatParameterWrapper : public Lv2ParameterWrapper
{
public:
    explicit Lv2FloatParameterWrapper(AudioParameterFloat *parameter)
        : parameter(parameter) {}
    Lv2ParameterType type() const override
        { return Lv2ParameterFloat; }
    float getValue() const override
        { return *parameter; }
    void setValue(float value) const override
        { *parameter = value; }
    float getDefault() const override
        { AudioProcessorParameter *baseParameter = static_cast<AudioProcessorParameter *>(parameter);
          return convertFrom0to1(baseParameter->getDefaultValue()); }
    float getMinimum() const override
        { return parameter->range.start; }
    float getMaximum() const override
        { return parameter->range.end; }
    float convertFrom0to1(float value) const override
        { return parameter->range.convertFrom0to1(value); }
    AudioParameterFloat *const parameter;
};

class Lv2IntParameterWrapper : public Lv2ParameterWrapper
{
public:
    explicit Lv2IntParameterWrapper(AudioParameterInt *parameter)
        : parameter(parameter) {}
    Lv2ParameterType type() const override
        { return Lv2ParameterInt; }
    float getValue() const override
        { return *parameter; }
    void setValue(float value) const override
        { *parameter = roundToInt(value); }
    float getDefault() const override
        { AudioProcessorParameter *baseParameter = static_cast<AudioProcessorParameter *>(parameter);
          return convertFrom0to1(baseParameter->getDefaultValue()); }
    float getMinimum() const override
        { return parameter->getRange().getStart(); }
    float getMaximum() const override
        { return parameter->getRange().getEnd(); }
    float convertFrom0to1(float value) const override
        { Range<int> range = parameter->getRange();
          int minValue = range.getStart();
          int maxValue = range.getEnd();
          return roundToInt(minValue + (maxValue - minValue) * value); }
    AudioParameterInt *const parameter;
};

class Lv2BoolParameterWrapper : public Lv2ParameterWrapper
{
public:
    explicit Lv2BoolParameterWrapper(AudioParameterBool *parameter)
        : parameter(parameter) {}
    Lv2ParameterType type() const override
        { return Lv2ParameterBool; }
    float getValue() const override
        { return *parameter; }
    void setValue(float value) const override
        { *parameter = (value > 0.0f)/* (value >= 0.5f) */ ? 1.0f : 0.0f; }
    float getDefault() const override
        { AudioProcessorParameter *baseParameter = static_cast<AudioProcessorParameter *>(parameter);
          return (baseParameter->getDefaultValue() >= 0.5f) ? 1.0f : 0.0f; }
    float getMinimum() const override
        { return 0.0f; }
    float getMaximum() const override
        { return 1.0f; }
    float convertFrom0to1(float value) const override
        { return (value > 0.0f)/* (value >= 0.5f) */ ? 1.0f : 0.0f; }
    AudioParameterBool *const parameter;
};

class Lv2ChoiceParameterWrapper : public Lv2ParameterWrapper
{
public:
    explicit Lv2ChoiceParameterWrapper(AudioParameterChoice *parameter)
        : parameter(parameter) {}
    Lv2ParameterType type() const override
        { return Lv2ParameterChoice; }
    float getValue() const override
        { return *parameter; }
    void setValue(float value) const override
        { *parameter = roundToInt(value); }
    float getDefault() const override
        { AudioProcessorParameter *baseParameter = static_cast<AudioProcessorParameter *>(parameter);
          return convertFrom0to1(baseParameter->getDefaultValue()); }
    float getMinimum() const override
        { return 0.0f; }
    float getMaximum() const override
        { return parameter->choices.size() - 1; }
    float convertFrom0to1(float value) const override
        { return roundToInt(value * (parameter->choices.size() - 1)); }
    AudioParameterChoice *const parameter;
};

class Lv2GenericParameterWrapper : public Lv2ParameterWrapper
{
public:
    explicit Lv2GenericParameterWrapper(AudioProcessorParameter *parameter)
        : parameter(parameter) {}
    Lv2ParameterType type() const override
        { return Lv2ParameterGeneric; }
    float getValue() const override
        { return parameter->getValue(); }
    void setValue(float value) const override
        { if (value != parameter->getValue()) parameter->setValueNotifyingHost(value); }
    float getDefault() const override
        { return parameter->getDefaultValue(); }
    float getMinimum() const override
        { return 0.0f; }
    float getMaximum() const override
        { return 1.0f; }
    float convertFrom0to1(float value) const override
        { return value; }
    AudioProcessorParameter *const parameter;
};

static Lv2ParameterWrapper *createParameterWrapper (AudioProcessor* filter, int parameterIndex)
{
    AudioProcessorParameter *parameter = filter->getParameters()[parameterIndex];

    if (AudioParameterFloat *p = dynamic_cast<AudioParameterFloat *>(parameter))
        return new Lv2FloatParameterWrapper(p);

    if (AudioParameterInt *p = dynamic_cast<AudioParameterInt *>(parameter))
        return new Lv2IntParameterWrapper(p);

    if (AudioParameterBool *p = dynamic_cast<AudioParameterBool *>(parameter))
        return new Lv2BoolParameterWrapper(p);

    if (AudioParameterChoice *p = dynamic_cast<AudioParameterChoice *>(parameter))
        return new Lv2ChoiceParameterWrapper(p);

    return new Lv2GenericParameterWrapper(parameter);
}

static OwnedArray<Lv2ParameterWrapper> wrapParameters (AudioProcessor* filter)
{
    OwnedArray<Lv2ParameterWrapper> parameters;
    int numParameters = filter->getParameters().size();
    parameters.ensureStorageAllocated(numParameters);
    for (int i=0; i < numParameters; ++i)
        parameters.add (createParameterWrapper (filter, i));
    return parameters;
}

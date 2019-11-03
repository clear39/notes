

//  @   frameworks/av/services/audiopolicy/enginedefault/src/EngineInstance.cpp
EngineInstance *EngineInstance::getInstance()
{
    static EngineInstance instance;
    return &instance;
}


EngineInstance::EngineInstance()
{
}


template <>
AudioPolicyManagerInterface *EngineInstance::queryInterface() const
{
    return getEngine()->queryInterface<AudioPolicyManagerInterface>();
}


Engine *EngineInstance::getEngine() const
{
    static Engine engine;
    return &engine;
}




class Stream : public CSubsystemObject{

}

/*
<Component Name="voice_call" Type="Stream" Mapping="Name:AUDIO_STREAM_VOICE_CALL"/>
*/
// mappingValue 为 AUDIO_STREAM_VOICE_CALL
// instanceConfigurableElement = CComponent
Stream::Stream(const string &/*mappingValue*/,
               CInstanceConfigurableElement *instanceConfigurableElement,
               const CMappingContext &context, core::log::Logger &logger)
    : CSubsystemObject(instanceConfigurableElement, logger),
      mPolicySubsystem(static_cast<const PolicySubsystem *>(
                           instanceConfigurableElement->getBelongingSubsystem())),
      mPolicyPluginInterface(mPolicySubsystem->getPolicyPluginInterface())
{
    std::string name(context.getItem(MappingKeyName));

    if (not android::StreamTypeConverter::fromString(name, mId)) {
        LOG_ALWAYS_FATAL("Invalid Stream type name: %s, invalid XML structure file", name.c_str());
    }

    // Declares the strategy to audio policy engine
    mPolicyPluginInterface->addStream(name, mId);
}


bool Stream::sendToHW(string & /*error*/)
{
    Applicable params;
    blackboardRead(&params, sizeof(params));

    mPolicyPluginInterface->setVolumeProfileForStream(
                mId, static_cast<audio_stream_type_t>(params.volumeProfile));

    return true;

}
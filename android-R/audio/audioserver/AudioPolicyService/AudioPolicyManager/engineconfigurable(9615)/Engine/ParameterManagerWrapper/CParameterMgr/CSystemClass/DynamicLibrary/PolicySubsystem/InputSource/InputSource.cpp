class PARAMETER_EXPORT CFormattedSubsystemObject : public CSubsystemObject{};
class InputSource : public CFormattedSubsystemObject {};


//<Component Name="default" Type="InputSource" Mapping="Name:AUDIO_SOURCE_DEFAULT"/>
// mappingValue 为 AUDIO_SOURCE_DEFAULT
// instanceConfigurableElement = CComponent
InputSource::InputSource(const string &mappingValue,
                         CInstanceConfigurableElement *instanceConfigurableElement,
                         const CMappingContext &context, core::log::Logger &logger)
    : CFormattedSubsystemObject(instanceConfigurableElement,
                                logger,
                                mappingValue,
                                MappingKeyAmend1,
                                (MappingKeyAmendEnd - MappingKeyAmend1 + 1),
                                context),
      mPolicySubsystem(static_cast<const PolicySubsystem *>(
                           instanceConfigurableElement->getBelongingSubsystem())),
      mPolicyPluginInterface(mPolicySubsystem->getPolicyPluginInterface())
{
    std::string name(context.getItem(MappingKeyName));

    if(not android::SourceTypeConverter::fromString(name, mId)) {
        LOG_ALWAYS_FATAL("Invalid Input Source name: %s, invalid XML structure file", name.c_str());
    }
    // Declares the strategy to audio policy engine
    mPolicyPluginInterface->addInputSource(name, mId);
}

bool InputSource::sendToHW(string & /*error*/)
{
    uint32_t applicableInputDevice;
    blackboardRead(&applicableInputDevice, sizeof(applicableInputDevice));
    return mPolicyPluginInterface->setDeviceForInputSource(mId, applicableInputDevice);
}

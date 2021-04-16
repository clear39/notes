


#define PARAMETER_FRAMEWORK_PLUGIN_ENTRYPOINT_V1 ParameterFrameworkPluginEntryPointMagicV1

class CSubsystemLibrary
    : public CDefaultElementLibrary<TLoggingElementBuilderTemplate<CVirtualSubsystem>>
{
private:
    // Builder type (based on element's name attribute)
    std::string getBuilderType(const CXmlElement &xmlElement) const override
    {
        // Xml element's name attribute
        std::string type;
        // 存在属性 Type="Policy"
        xmlElement.getAttribute("Type", type);
        return type;
    }
};

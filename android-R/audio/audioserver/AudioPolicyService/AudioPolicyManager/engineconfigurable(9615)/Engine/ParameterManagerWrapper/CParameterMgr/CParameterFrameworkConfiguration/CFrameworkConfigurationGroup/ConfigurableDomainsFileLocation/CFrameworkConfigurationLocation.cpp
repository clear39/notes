

class CFrameworkConfigurationLocation : public CKindElement
{
public:
    CFrameworkConfigurationLocation(const std::string &strName, const std::string &strKind);

    /** Get Configuration file URI
     */
    const std::string &getUri() const;

    // From IXmlSink
    bool fromXml(const CXmlElement &xmlElement,
                 CXmlSerializingContext &serializingContext) override;

private:
    std::string _configurationUri;
};


// From IXmlSink
bool CFrameworkConfigurationLocation::fromXml(const CXmlElement &xmlElement,
                                              CXmlSerializingContext &serializingContext)
{
    xmlElement.getAttribute("Path", _configurationUri);

    if (_configurationUri.empty()) {

        serializingContext.setError("Empty Path attribute in element " + xmlElement.getPath());

        return false;
    }
    return true;
}


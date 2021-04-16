

class CFileIncluderElementBuilder : public CElementBuilder
{
public:
    CFileIncluderElementBuilder(bool bValidateWithSchemas, const std::string &schemaBaseUri)
        : CElementBuilder(), _bValidateWithSchemas(bValidateWithSchemas),
          _schemaBaseUri(schemaBaseUri)
    {
    }

    CElement *createElement(const CXmlElement &xmlElement) const override
    {
        return new CXmlFileIncluderElement(xmlElement.getNameAttribute(), xmlElement.getType(),
                                           _bValidateWithSchemas, _schemaBaseUri);
    }

private:
    bool _bValidateWithSchemas;
    const std::string _schemaBaseUri;
};
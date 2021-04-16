
// @ external/parameter-framework/upstream/parameter/XmlParameterSerializingContext.h
class CXmlParameterSerializingContext : public CXmlElementSerializingContext
{
public:
    CXmlParameterSerializingContext(CParameterAccessContext &context, std::string &strError);

    // ComponentLibrary
    void setComponentLibrary(const CComponentLibrary *pComponentLibrary);
    const CComponentLibrary *getComponentLibrary() const;

    CParameterAccessContext &getAccessContext() const { return mAccessContext; }

private:
    const CComponentLibrary *_pComponentLibrary{nullptr};

    CParameterAccessContext &mAccessContext;
};


CXmlParameterSerializingContext::CXmlParameterSerializingContext(CParameterAccessContext &context, string &strError)
    : base(strError), mAccessContext(context)
{
}

// ComponentLibrary
void CXmlParameterSerializingContext::setComponentLibrary(const CComponentLibrary *pComponentLibrary)
{
    _pComponentLibrary = pComponentLibrary;
}

const CComponentLibrary *CXmlParameterSerializingContext::getComponentLibrary() const
{
    return _pComponentLibrary;
}


/*
<!--对应创建 CFrameworkConfigurationGroup -->
<SettingsConfiguration>
    <!--对应创建 ConfigurableDomainsFileLocation -->
    <ConfigurableDomainsFileLocation Path="Settings/Policy/PolicyConfigurableDomains.xml"/>
</SettingsConfiguration>
*/

class CFrameworkConfigurationGroup : public CKindElement
{
public:
    CFrameworkConfigurationGroup(const std::string &strName, const std::string &strKind)
        : CKindElement(strName, strKind)
    {
    }

private:
    bool childrenAreDynamic() const override { return true; }
};


// From IXmlSink
bool CElement::fromXml(const CXmlElement &xmlElement,
                       CXmlSerializingContext &serializingContext) try {
	
    xmlElement.getAttribute(gDescriptionPropertyName, _strDescription);

    // Propagate through children
    CXmlElement::CChildIterator childIterator(xmlElement);

    CXmlElement childElement;

    while (childIterator.next(childElement)) {

        CElement *pChild;

        // CFrameworkConfigurationGroup 重载 childrenAreDynamic 返回 true
        if (!childrenAreDynamic()) {

            pChild = findChildOfKind(childElement.getType());

            if (!pChild) {

                serializingContext.setError("Unable to handle XML element: " +
                                            childElement.getPath());

                return false;
            }

        } else {
            // Child needs creation
            pChild = createChild(childElement, serializingContext);

            if (!pChild) {

                return false;
            }
        }

        // Dig
        if (!pChild->fromXml(childElement, serializingContext)) {

            return false;
        }
    }

    return true;
} catch (const PfError &e) {
    serializingContext.appendLineToError(e.what());
    return false;
}
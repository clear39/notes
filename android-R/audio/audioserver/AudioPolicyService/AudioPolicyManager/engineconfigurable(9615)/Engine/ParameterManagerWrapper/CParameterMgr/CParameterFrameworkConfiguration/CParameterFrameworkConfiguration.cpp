

//这里是在 bool CXmlMemoryDocSink::doProcess(CXmlDocSource &xmlDocSource, CXmlSerializingContext &serializingContext)
//中调用
// From IXmlSink
//这里解析 ParameterFrameworkConfigurationPolicy.xml
bool CParameterFrameworkConfiguration::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
{
    /*
    SystemClassName="Policy" 
    ServerPort="5019"
    TuningAllowed="false"
    */
    // System class name
    xmlElement.getAttribute("SystemClassName", _strSystemClassName);

    // Tuning allowed
    xmlElement.getAttribute("TuningAllowed", _bTuningAllowed);

    // Server port
    xmlElement.getAttribute("ServerPort", _bindAddress);

    // Base
    //对应 CElement::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
    return base::fromXml(xmlElement, serializingContext);
}

// From IXmlSink
bool CElement::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext) try {
    //const std::string CElement::gDescriptionPropertyName = "Description";
    xmlElement.getAttribute(gDescriptionPropertyName, _strDescription);

    // Propagate through children
    CXmlElement::CChildIterator childIterator(xmlElement);

    CXmlElement childElement;

    while (childIterator.next(childElement)) {

        CElement *pChild;

        //由于CParameterFrameworkConfiguration重载了childrenAreDynamic
        //且返回 true
        if (!childrenAreDynamic()) {

            pChild = findChildOfKind(childElement.getType());

            if (!pChild) {

                serializingContext.setError("Unable to handle XML element: " +
                                            childElement.getPath());

                return false;
            }

        } else {
            //执行这里
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

CElement *CElement::createChild(const CXmlElement &childElement, CXmlSerializingContext &serializingContext)
{
    // serializingContext 原始创建类为 CXmlElementSerializingContext
    // Context
    CXmlElementSerializingContext &elementSerializingContext =
        static_cast<CXmlElementSerializingContext &>(serializingContext);

    // elementSerializingContext.getElementLibrary() 中得到 
    // 在CParameterMgr::feedElementLibraries中添加的第一个 CElementLibrary
    // 调用 CElementLibrary->createElement
    // Child needs creation
    // 这里通过 createElement 方法得到 CSubsystemPlugins
    // CSubsystemPlugins 构造方法为 
    CElement *pChild = elementSerializingContext.getElementLibrary()->createElement(childElement);

    if (!pChild) {
        elementSerializingContext.setError("Unable to create XML element " + childElement.getPath());
        return nullptr;
    }

    // Store created child!
    addChild(pChild);

    return pChild;
}




// System class name
const std::string &CParameterFrameworkConfiguration::getSystemClassName() const
{
    //在 CParameterFrameworkConfiguration::fromXml 中解析得到
    return _strSystemClassName;
}



// Tuning allowed
bool CParameterFrameworkConfiguration::isTuningAllowed() const
{
    //在 CParameterFrameworkConfiguration::fromXml 中解析得到
    return _bTuningAllowed;
}


// Server bind address
const std::string &CParameterFrameworkConfiguration::getServerBindAddress() const
{
    //在 CParameterFrameworkConfiguration::fromXml 中解析得到
    return _bindAddress;
}

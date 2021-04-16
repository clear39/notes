


CXmlFileIncluderElement::CXmlFileIncluderElement(const std::string &strName,
                                                 const std::string &strKind,
                                                 bool bValidateWithSchemas,
                                                 const std::string &schemaBaseUri)
    : base(strName, strKind), _bValidateSchemasOnStart(bValidateWithSchemas),
      _schemaBaseUri(schemaBaseUri)
{
}

// From IXmlSink
bool CXmlFileIncluderElement::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
{
    // Context
    CXmlElementSerializingContext &elementSerializingContext =
        static_cast<CXmlElementSerializingContext &>(serializingContext);

    // Parse included document
    std::string strPath;
    // strPath 为 PolicySubsystem.xml
    xmlElement.getAttribute("Path", strPath);
    // strPath 为 /vendor/etc/parameter-framework/Structure/Policy/PolicySubsystem.xml
    strPath = CXmlDocSource::mkUri(elementSerializingContext.getXmlUri(), strPath);

    // Instantiate parser
    // getIncludedElementType 将 "SubsystemInclude" 去掉 "Include" 得到 "Subsystem"
    // strIncludedElementType = "Subsystem"
    std::string strIncludedElementType = getIncludedElementType();
    {
        _xmlDoc *doc = CXmlDocSource::mkXmlDoc(strPath, true, true, elementSerializingContext);

        // _bValidateSchemasOnStart 为 false
        CXmlDocSource docSource(doc, _bValidateSchemasOnStart, strIncludedElementType);

        //  _schemaBaseUri 为空
        if (not _schemaBaseUri.empty()) {
            docSource.setSchemaBaseUri(_schemaBaseUri);
        }

        if (!docSource.isParsable()) {
            elementSerializingContext.setError("Could not parse document \"" + strPath + "\"");
            return false;
        }

        // Get top level element
        CXmlElement childElement;

        // docSource 代表 /vendor/etc/parameter-framework/Structure/Policy/PolicySubsystem.xml
        docSource.getRootElement(childElement);

        // Create child element
        // getElementLibrary() 得到 _pElementLibrarySet 的第二成员 CElementLibrary
        // 通过 createElement中 再次调用 CSubsystemElementBuilder.createElement(childElement)
        // CSubsystemElementBuilder::createElement 中调用 getSystemClass()->getSubsystemLibrary()->createElement(childElement)
        // 即调用 CSubsystemLibrary->createElement(childElement)
        // getSubsystemLibrary 得到 _pSubsystemLibrary(通过CSystemClass::loadSubsystems) 中添加的库集合
        // _pSubsystemLibrary->addElementBuilder("Virtual", new VirtualSubsystemBuilder(_logger));
        // _pSubsystemLibrary->addElementBuilder("Policy", new TLoggingElementBuilderTemplate<PolicySubsystem>(logger));
        // 再次调用 TLoggingElementBuilderTemplate->createElement(xmlElement)
        // 在 TLoggingElementBuilderTemplate->createElement(xmlElement) 中 new PolicySubsystem(details::getName(xmlElement),mLogger)
        // details::getName(xmlElement) 获取  Name="policy" 
        // 所以这里的 pChild = new PolicySubsystem(details::getName(xmlElement),mLogger);
        // pChild = new PolicySubsystem("policy",mLogger);
        CElement *pChild = elementSerializingContext.getElementLibrary()->createElement(childElement);

        if (pChild) {
            // Store created child!
            // 将 PolicySubsystem 添加到 CSystemClass中
            getParent()->addChild(pChild);
        } else {

            elementSerializingContext.setError("Unable to create XML element " + childElement.getPath());

            return false;
        }

        // Use a doc sink that instantiate the structure from the doc source
        // pChild 为 PolicySubsystem
        CXmlMemoryDocSink memorySink(pChild);

        // 内部会调用 CXmlMemoryDocSink::doProcess 
        // 在 CXmlMemoryDocSink::doProcess 中 PolicySubsystem->fromXml
        // 由于 PolicySubsystem 未实现 fromXml,直接调用 父类的父类 CConfigurableElement::fromXml
        // 这里解析的是 /vendor/etc/parameter-framework/Structure/Policy/PolicySubsystem.xml 
        if (!memorySink.process(docSource, elementSerializingContext)) {

            return false;
        }
    }

    //从 CSystemClass 移除掉 CXmlFileIncluderElement
    // Detach from parent
    getParent()->removeChild(this);

    // Self destroy
    delete this;

    return true;
}


// Element type
std::string CXmlFileIncluderElement::getIncludedElementType() const
{
    std::string strKind = getKind(); //"SubsystemInclude"

    std::string::size_type pos = strKind.rfind("Include", std::string::npos);

    assert(pos != std::string::npos);

    return strKind.substr(0, pos);//"Subsystem"
}
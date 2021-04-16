
class CSystemClass final : public CConfigurableElement {

}

//	@	external/parameter-framework/upstream/parameter/SystemClass.cpp
//在CParameterMgr构造函数中创建
CSystemClass::CSystemClass(log::Logger &logger)
    : _pSubsystemLibrary(new CSubsystemLibrary()), _logger(logger)
{
}

/**
 * 在 bool CParameterMgr::loadSubsystems(std::string &error) 触发
*/
bool CSystemClass::loadSubsystems(string &strError, const CSubsystemPlugins *pSubsystemPlugins,
                                  bool bVirtualSubsystemFallback)
{
    // clean清空一下map数组
    // Start clean
    _pSubsystemLibrary->clean();

    typedef TLoggingElementBuilderTemplate<CVirtualSubsystem> VirtualSubsystemBuilder;
    // Add virtual subsystem builder
    _pSubsystemLibrary->addElementBuilder("Virtual", new VirtualSubsystemBuilder(_logger));
    // Set virtual subsytem as builder fallback if required
    if (bVirtualSubsystemFallback) {  // bVirtualSubsystemFallback 为 false
        _pSubsystemLibrary->setDefaultBuilder(
            utility::make_unique<VirtualSubsystemBuilder>(_logger));
    }

    // Add subsystem defined in shared libraries
    core::Results errors;
    bool bLoadPluginsSuccess = loadSubsystemsFromSharedLibraries(errors, pSubsystemPlugins);

    // Fill strError for caller, he has to decide if there is a problem depending on
    // bVirtualSubsystemFallback value
    strError = utility::asString(errors);

    return bLoadPluginsSuccess || bVirtualSubsystemFallback;
}

bool CSystemClass::loadSubsystemsFromSharedLibraries(core::Results &errors,
                                                     const CSubsystemPlugins *pSubsystemPlugins)
{
    // Plugin list
    list<string> lstrPluginFiles;

    size_t pluginLocation;

    for (pluginLocation = 0; pluginLocation < pSubsystemPlugins->getNbChildren(); pluginLocation++) {

        // Get Folder for current Plugin Location
        const CPluginLocation *pPluginLocation =
            static_cast<const CPluginLocation *>(pSubsystemPlugins->getChild(pluginLocation));

        string strFolder(pPluginLocation->getFolder());
        if (!strFolder.empty()) {
            strFolder += "/";
        }
        // Iterator on Plugin List:
        list<string>::const_iterator it;

        const list<string> &pluginList = pPluginLocation->getPluginList();

        for (it = pluginList.begin(); it != pluginList.end(); ++it) {

            // Fill Plugin files list
            lstrPluginFiles.push_back(strFolder + *it);
        }
    }

    // Actually load plugins
    while (!lstrPluginFiles.empty()) {

        // Because plugins might depend on one another, loading will be done
        // as an iteration process that finishes successfully when the remaining
        // list of plugins to load gets empty or unsuccessfully if the loading
        // process failed to load at least one of them

        // Attempt to load the complete list
        if (!loadPlugins(lstrPluginFiles, errors)) {

            // Unable to load at least one plugin
            break;
        }
    }

    if (!lstrPluginFiles.empty()) {
        // Unable to load at least one plugin
        errors.push_back("Unable to load the following plugins: " +
                         utility::asString(lstrPluginFiles, ", ") + ".");
        return false;
    }

    return true;
}


// Plugin loading
bool CSystemClass::loadPlugins(list<string> &lstrPluginFiles, core::Results &errors)
{
    assert(lstrPluginFiles.size());

    bool bAtLeastOneSubsystemPluginSuccessfullyLoaded = false;

    auto it = lstrPluginFiles.begin();

    while (it != lstrPluginFiles.end()) {

        string strPluginFileName = *it;

        // Load attempt
        try {
            auto library = utility::make_unique<DynamicLibrary>(strPluginFileName);

            // const char CSystemClass::entryPointSymbol[] = MACRO_TO_STR(PARAMETER_FRAMEWORK_PLUGIN_ENTRYPOINT_V1);
            // Load symbol from library
            auto subSystemBuilder = library->getSymbol<PluginEntryPointV1>(entryPointSymbol);

            // Store libraries handles
            _subsystemLibraryHandleList.push_back(std::move(library));

            // 这里执行的是 libpolicy-subsystem.so 中的 PARAMETER_FRAMEWORK_PLUGIN_ENTRYPOINT_V1
            // frameworks/av/services/audiopolicy/engineconfigurable/parameter-framework/plugin/PolicySubsystemBuilder.cpp
            // Fill library
            //static const char *const POLICY_SUBSYSTEM_NAME = "Policy";
            //内部相当于执行
            // _pSubsystemLibrary->addElementBuilder(POLICY_SUBSYSTEM_NAME,new TLoggingElementBuilderTemplate<PolicySubsystem>(logger));
            subSystemBuilder(_pSubsystemLibrary, _logger);

        } catch (std::exception &e) {
            errors.push_back(e.what());

            // Next plugin
            ++it;

            continue;
        }

        // Account for this success
        bAtLeastOneSubsystemPluginSuccessfullyLoaded = true;

        // Remove successfully loaded plugin from list and select next
        lstrPluginFiles.erase(it++);
    }

    return bAtLeastOneSubsystemPluginSuccessfullyLoaded;
}


//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
// 这里是由CParameterMgr::loadStructure 触发调用
// 由于 CSystemClass 没有重写fromXml
bool CConfigurableElement::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
{
    auto &context = static_cast<CXmlParameterSerializingContext &>(serializingContext);
    // accessContext 为 CParameterAccessContext 
    auto &accessContext = context.getAccessContext();

    // accessContext.serializeSettings() 返回 false 
    if (accessContext.serializeSettings()) {
        // As serialization and deserialisation are handled through the *same* function
        // the (de)serialize object can not be const in `serializeXmlSettings` signature.
        // As a result a const_cast is unavoidable :(.
        // Fixme: split serializeXmlSettings in two functions (in and out) to avoid the `const_cast`
        return serializeXmlSettings(const_cast<CXmlElement &>(xmlElement),
                                    static_cast<CConfigurationAccessContext &>(accessContext));
    }
    return structureFromXml(xmlElement, serializingContext);
}

/** Deserialize the structure from xml. */
bool CConfigurableElement::structureFromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
{
    // Forward to Element::fromXml.
    // This is unfortunate as Element::fromXml will call back
    // fromXml on each children.
    // Thus on each non leaf node of the tree, the code will test if
    // the setting or the structure are to be serialized.
    // This test could be avoided by several ways including:
    //  - split 2 roles fromXml in two function
    //    1) construct the elements
    //    2) recursive call on children
    //  - dispatch in with a virtual method. This would not not remove
    //    the branching rather hide it behind a virtual method override.
    return CElement::fromXml(xmlElement, serializingContext);
}

// From IXmlSink
bool CElement::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext) try {
    xmlElement.getAttribute(gDescriptionPropertyName, _strDescription);

    // Propagate through children
    CXmlElement::CChildIterator childIterator(xmlElement);

    CXmlElement childElement;

    while (childIterator.next(childElement)) {

        CElement *pChild;

        // CSystemClass::childrenAreDynamic() 返回 true 
        if (!childrenAreDynamic()) {

            pChild = findChildOfKind(childElement.getType());
            if (!pChild) {
                serializingContext.setError("Unable to handle XML element: " + childElement.getPath());
                return false;
            }

        } else {

            // 执行这里
            // Child needs creation
            pChild = createChild(childElement, serializingContext);
            if (!pChild) {
                return false;
            }

        }

        // Dig  只有一个成员为 CFileIncluderElementBuilder
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
    // Context
    CXmlElementSerializingContext &elementSerializingContext =
        static_cast<CXmlElementSerializingContext &>(serializingContext);
p
    // elementSerializingContext.getElementLibrary() 得到 CParameterMgr::feedElementLibraries 
    // 中的第二个 CElementLibrary, 
    // createElement 创建 SubsystemInclude 标签对应的 CFileIncluderElementBuilder
    // Child needs creation
    CElement *pChild = elementSerializingContext.getElementLibrary()->createElement(childElement);

    if (!pChild) {
        elementSerializingContext.setError("Unable to create XML element " + childElement.getPath());
        return nullptr;
    }

    // Store created child!
    addChild(pChild);

    return pChild;
}


const CSubsystemLibrary *CSystemClass::getSubsystemLibrary() const
{
    return _pSubsystemLibrary;
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
// Offset
void CConfigurableElement::setOffset(size_t offset)
{
    // Assign offset locally
    _offset = offset;

    // Propagate to children
    // 只有一个孩子成员 PolicySubsystem 
    size_t uiNbChildren = getNbChildren();

    for (size_t index = 0; index < uiNbChildren; index++) {

        CConfigurableElement *pConfigurableElement =
            static_cast<CConfigurableElement *>(getChild(index));

        // PolicySubsystem 
        pConfigurableElement->setOffset(offset);

        offset += pConfigurableElement->getFootPrint();
    }
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
// Memory
size_t CConfigurableElement::getFootPrint() const
{
    size_t uiSize = 0;
     // 只有一个孩子成员 PolicySubsystem 
    size_t uiNbChildren = getNbChildren();

    for (size_t index = 0; index < uiNbChildren; index++) {

        const CConfigurableElement *pConfigurableElement =
            static_cast<const CConfigurableElement *>(getChild(index));

        uiSize += pConfigurableElement->getFootPrint();
    }

    return uiSize;
}


//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

const CElement *CElement::findDescendant(CPathNavigator &pathNavigator) const
{
    // policy/product_strategies/transmitted_through_speaker/selected_output_devices/mask/speaker
    string *pStrChildName = pathNavigator.next(); // pStrChildName = policy

    if (!pStrChildName) {

        return this;
    }

    // pChild = PolicySubsystem
    const CElement *pChild = findChild(*pStrChildName);

    if (!pChild) {

        return nullptr;
    }

    // pChild = PolicySubsystem
    return pChild->findDescendant(pathNavigator);
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
// CSystemClass 中的成员 需要特别注意： 在 CParameterMgr::loadStructure 中加载的是成员类CXmlFileIncluderElement，
// 后面在 CXmlFileIncluderElement::fromXml中进行移除，然后重新添加了 PolicySubsystem
// typedef std::list<std::string> Results;
void CSystemClass::checkForSubsystemsToResync(CSyncerSet &syncerSet, core::Results &infos)
{
	// CSystemClass 就一个成员为 PolicySubsystem
    size_t uiNbChildren = getNbChildren();
    size_t uiChild;

    for (uiChild = 0; uiChild < uiNbChildren; uiChild++) {
        CSubsystem *pSubsystem = static_cast<CSubsystem *>(getChild(uiChild));

        // pSubsystem 为 PolicySubsystem 
        // Collect and consume the need for a resync
        // 由于 needResync 始终返回 false
        if (pSubsystem->needResync(true)) {
            infos.push_back("Resynchronizing subsystem: " + pSubsystem->getName());
            // get all subsystem syncers
            pSubsystem->fillSyncerSet(syncerSet);
        }
    } 
}
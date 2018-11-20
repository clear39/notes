//  @system/libvintf/VintfObject.cpp
// static
const HalManifest *VintfObject::GetDeviceHalManifest(bool skipCache =  false) {
	//	static LockedUniquePtr<HalManifest> gDeviceManifest;
    return Get(&gDeviceManifest, skipCache,std::bind(&HalManifest::fetchAllInformation, std::placeholders::_1,"/vendor/manifest.xml"));
}

//  @system/libvintf/VintfObject.cpp
// static
const HalManifest *VintfObject::GetFrameworkHalManifest(bool skipCache =  false) {
	//	static LockedUniquePtr<HalManifest> gFrameworkManifest;
    return Get(&gFrameworkManifest, skipCache,std::bind(&HalManifest::fetchAllInformation, std::placeholders::_1,"/system/manifest.xml"));
}


//  @system/libvintf/VintfObject.cpp
template <typename T, typename F>
static const T *Get(LockedUniquePtr<T> *ptr,bool skipCache,const F &fetchAllInformation) {
    std::unique_lock<std::mutex> _lock(ptr->mutex);
    if (skipCache || ptr->object == nullptr) {
        ptr->object = std::make_unique<T>();//std::make_unique<HalManifest>();
        if (fetchAllInformation(ptr->object.get()) != OK) {//   fetchAllInformation = HalManifest::fetchAllInformation
            ptr->object = nullptr; // frees the old object
        }
    }
    return ptr->object.get();
}


status_t HalManifest::fetchAllInformation(const std::string &path) {
	//	@system/libvintf/parse_xml.cpp:817:const HalManifestConverter halManifestConverter{};
	//	@system/libvintf/parse_xml.cpp:946:const XmlConverter<HalManifest> &gHalManifestConverter = halManifestConverter;
    return details::fetchAllInformation(path, gHalManifestConverter, this);
}



struct HalManifestConverter : public XmlNodeConverter<HalManifest> {

     bool buildObject(HalManifest *object, NodeType *root) const override {
        Version version;
        std::vector<ManifestHal> hals;
        if (!parseAttr(root, "version", &version) ||
            !parseAttr(root, "type", &object->mType) ||
            !parseChildren(root, manifestHalConverter, &hals)) {
            return false;
        }
        if (version != HalManifest::kVersion) {//   constexpr static Version kVersion{1, 0};
            this->mLastError = "Unrecognized manifest.version";
            return false;
        }
        if (object->mType == SchemaType::DEVICE) {
            // tags for device hal manifest only.
            // <sepolicy> can be missing because it can be determined at build time, not hard-coded
            // in the XML file.
            if (!parseOptionalChild(root, halManifestSepolicyConverter, {},&object->device.mSepolicyVersion)) {
                return false;
            }
        } else if (object->mType == SchemaType::FRAMEWORK) {
            if (!parseChildren(root, vndkConverter, &object->framework.mVndks)) {
                return false;
            }
            for (const auto &vndk : object->framework.mVndks) {
                if (!vndk.mVersionRange.isSingleVersion()) {
                    this->mLastError = "vndk.version " + to_string(vndk.mVersionRange) + " cannot be a range for manifests";
                    return false;
                }
            }
        }

        for (auto &&hal : hals) {
            std::string description{hal.name};
            if (!object->add(std::move(hal))) {
                this->mLastError = "Duplicated manifest.hal entry " + description;
                return false;
            }
        }

        std::vector<ManifestXmlFile> xmlFiles;
        if (!parseChildren(root, manifestXmlFileConverter, &xmlFiles)) {
            return false;
        }

        for (auto&& xmlFile : xmlFiles) {
            std::string description{xmlFile.name()};
            if (!object->addXmlFile(std::move(xmlFile))) {
                this->mLastError = "Duplicated manifest.xmlfile entry " + description + "; entries cannot have duplicated name and version";
                return false;
            }
        }

        return true;
    }
}


/**
using NodeType = tinyxml2::XMLElement;
using DocType = tinyxml2::XMLDocument;
*/
//	@system/libvintf/parse_xml.cpp:136
template<typename Object>
struct XmlNodeConverter : public XmlConverter<Object> {

    inline bool operator()(Object *o, const std::string &xml) const {
        return deserialize(o, xml);
    }


	 inline bool deserialize(Object *o, const std::string &xml) const {
        DocType *doc = createDocument(xml);
        if (doc == nullptr) {
            this->mLastError = "Not a valid XML";
            return false;
        }
        bool ret = deserialize(o, getRootChild(doc));
        deleteDocument(doc);
        return ret;
    }

    // caller is responsible for deleteDocument() call
	inline DocType *createDocument(const std::string &xml) {
	    DocType *doc = new tinyxml2::XMLDocument();
	    if (doc->Parse(xml.c_str()) == tinyxml2::XML_NO_ERROR) {
	        return doc;
	    }
	    delete doc;
	    return nullptr;
	}


    inline NodeType *getRootChild(DocType *parent) {
	    return parent->FirstChildElement();
	}


	inline bool deserialize(Object *object, NodeType *root) const {
        if (nameOf(root) != this->elementName()) {
            return false;
        }
        return this->buildObject(object, root);
    }


    template <typename T>
    inline bool parseChildren(NodeType *root, const XmlNodeConverter<T> &conv, std::vector<T> *v) const {
        auto nodes = getChildren(root, conv.elementName());
        v->resize(nodes.size());
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (!conv.deserialize(&v->at(i), nodes[i])) {
                mLastError = "Could not parse element with name <" + conv.elementName()  + "> in element <" + this->elementName() + ">: " + conv.lastError();
                return false;
            }
        }
        return true;
    }


}

//  @system/libvintf/utils.h
template <typename T>
status_t fetchAllInformation(const std::string& path, const XmlConverter<T>& converter,T* outObject) {
    std::string info;

    if (gFetcher == nullptr) {
        // Should never happen.
        return NO_INIT;
    }

    status_t result = gFetcher->fetch(path, info);//读取path内容到info

    if (result != OK) {
        return result;
    }

    bool success = converter(outObject, info);//调用括号符号重载函数
    if (!success) {
        LOG(ERROR) << "Illformed file: " << path << ": " << converter.lastError();
        return BAD_VALUE;
    }
    return OK;
}


// Return the file from the given location as a string.
//
// This class can be used to create a mock for overriding.
class FileFetcher {
   public:
    virtual ~FileFetcher() {}
    virtual status_t fetch(const std::string& path, std::string& fetched) {
        std::ifstream in;

        in.open(path);
        if (!in.is_open()) {
            LOG(WARNING) << "Cannot open " << path;
            return INVALID_OPERATION;
        }

        std::stringstream ss;
        ss << in.rdbuf();
        fetched = ss.str();

        return OK;
    }
};





//	@system/libvintf/HalManifest.cpp
Transport HalManifest::getTransport(const std::string &package, const Version &v,const std::string &interfaceName, const std::string &instanceName) const {

    for (const ManifestHal *hal : getHals(package)) {
        bool found = false;
        for (auto& ver : hal->versions) {
            if (ver.majorVer == v.majorVer && ver.minorVer >= v.minorVer) {
                found = true;
                break;
            }
        }
        if (!found) {
            LOG(DEBUG) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot find " << to_string(v) << " in supported versions of " << package;
            continue;
        }
        auto it = hal->interfaces.find(interfaceName);
        if (it == hal->interfaces.end()) {
            LOG(DEBUG) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot find interface '" << interfaceName << "' in " << package << "@" << to_string(v);
            continue;
        }
        const auto &instances = it->second.instances;
        if (instances.find(instanceName) == instances.end()) {
            LOG(DEBUG) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot find instance '" << instanceName << "' in "  << package << "@" << to_string(v) << "::" << interfaceName;
            continue;
        }
        return hal->transportArch.transport;
    }
    LOG(DEBUG) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot get transport for " << package << "@" << v << "::" << interfaceName << "/" << instanceName;
    return Transport::EMPTY;

}
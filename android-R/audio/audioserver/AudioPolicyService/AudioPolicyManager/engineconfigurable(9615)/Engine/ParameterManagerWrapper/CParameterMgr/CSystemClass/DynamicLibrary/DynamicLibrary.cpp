class DynamicLibrary : private utility::NonCopyable{

}



const std::string DynamicLibrary::_osLibraryPrefix = "lib";
const std::string DynamicLibrary::_osLibrarySuffix = ".so";

DynamicLibrary::DynamicLibrary(const std::string &path) : _path(osSanitizePathName(path))
{
    _handle = dlopen(_path.c_str(), RTLD_LAZY);

    if (_handle == nullptr) {

        const char *dlError = dlerror();
        throw std::runtime_error((dlError != nullptr) ? dlError : "unknown dlopen error");
    }
}


std::string DynamicLibrary::osSanitizePathName(const std::string &path)
{
    if (path.rfind(_osLibrarySuffix) == (path.length() - _osLibrarySuffix.length())) {

        return path;
    }

    std::string sanitizedPath = _osLibraryPrefix + path + _osLibrarySuffix;

    return sanitizedPath;
}

//SymbolType = PluginEntryPointV1
//symbol 为 const char CSystemClass::entryPointSymbol[] = MACRO_TO_STR(PARAMETER_FRAMEWORK_PLUGIN_ENTRYPOINT_V1);
//#define PARAMETER_FRAMEWORK_PLUGIN_ENTRYPOINT_V1 ParameterFrameworkPluginEntryPointMagicV1
//#define QUOTE(X) #X
//#define MACRO_TO_STR(X) QUOTE(X)
template <typename SymbolType>
SymbolType getSymbol(const std::string &symbol) const
{
    return reinterpret_cast<SymbolType>(osGetSymbol(symbol));
}

// symbol = ParameterFrameworkPluginEntryPointMagicV1
void *DynamicLibrary::osGetSymbol(const std::string &symbol) const
{
    void *sym = dlsym(_handle, symbol.c_str());

    if (sym == nullptr) {

        const char *dlError = dlerror();
        throw std::runtime_error((dlError != nullptr) ? dlError : "unknown dlsym error");
    }

    return sym;
}


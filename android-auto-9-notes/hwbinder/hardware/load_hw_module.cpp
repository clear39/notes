//	@hardware/libhardware/hardware.c
/*
这个方法主要是去

#if defined(__LP64__)
#define HAL_LIBRARY_PATH1 "/system/lib64/hw"
#define HAL_LIBRARY_PATH2 "/vendor/lib64/hw"
#define HAL_LIBRARY_PATH3 "/odm/lib64/hw"
#else
#define HAL_LIBRARY_PATH1 "/system/lib/hw"
#define HAL_LIBRARY_PATH2 "/vendor/lib/hw"
#define HAL_LIBRARY_PATH3 "/odm/lib/hw"

目录下查找由 id.prop.so 文件,prop属性有如下:
    "ro.hardware",  
    "ro.product.board",
    "ro.board.platform",
    "ro.arch"

查找目录优先级 odm > vendor > system

查找属性优先级 "ro.hardware",  >   "ro.product.board", >  '"ro.board.platform" > "ro.arch"

*/
int hw_get_module(const char *id, const struct hw_module_t **module)
{
    return hw_get_module_by_class(id, NULL, module);
}



int hw_get_module_by_class(const char *class_id, const char *ins = NULL,const struct hw_module_t **module)
{
    int i = 0;
    char prop[PATH_MAX] = {0};
    char path[PATH_MAX] = {0};
    char name[PATH_MAX] = {0};
    char prop_name[PATH_MAX] = {0};


    if (inst)
        snprintf(name, PATH_MAX, "%s.%s", class_id, inst);
    else
        strlcpy(name, class_id, PATH_MAX);

    /*
     * Here we rely on the fact that calling dlopen multiple times on
     * the same .so will simply increment a refcount (and not load
     * a new copy of the library).
     * We also assume that dlopen() is thread-safe.
     */

    /* First try a property specific to the class and possibly instance */
    snprintf(prop_name, sizeof(prop_name), "ro.hardware.%s", name);//	ro.hardware.(class_id)
    if (property_get(prop_name, prop, NULL) > 0) {
        if (hw_module_exists(path, sizeof(path), name, prop) == 0) {
            goto found;
        }
    }

    /*
    static const char *variant_keys[] = {
    "ro.hardware",  // This goes first so that it can pick up a differentfile on the emulator. 
    "ro.product.board",
    "ro.board.platform",
    "ro.arch"
};

static const int HAL_VARIANT_KEYS_COUNT = (sizeof(variant_keys)/sizeof(variant_keys[0]));
    */

    /* Loop through the configuration variants looking for a module */
    for (i=0 ; i<HAL_VARIANT_KEYS_COUNT; i++) {
        if (property_get(variant_keys[i], prop, NULL) == 0) {
            continue;
        }
        if (hw_module_exists(path, sizeof(path), name, prop) == 0) {
            goto found;
        }
    }

    /* Nothing found, try the default */
    if (hw_module_exists(path, sizeof(path), name, "default") == 0) {
        goto found;
    }

    return -ENOENT;

found:
    /* load the module, if this fails, we're doomed, and we should not try
     * to load a different variant. */
    return load(class_id, path, module);
}


/** Base path of the hal modules */
#if defined(__LP64__)
#define HAL_LIBRARY_PATH1 "/system/lib64/hw"
#define HAL_LIBRARY_PATH2 "/vendor/lib64/hw"
#define HAL_LIBRARY_PATH3 "/odm/lib64/hw"
#else
#define HAL_LIBRARY_PATH1 "/system/lib/hw"
#define HAL_LIBRARY_PATH2 "/vendor/lib/hw"
#define HAL_LIBRARY_PATH3 "/odm/lib/hw"
#endif


/*
 * Check if a HAL with given name and subname exists, if so return 0, otherwise
 * otherwise return negative.  On success path will contain the path to the HAL.
 */
static int hw_module_exists(char *path, size_t path_len, const char *name,const char *subname)
{
    snprintf(path, path_len, "%s/%s.%s.so", HAL_LIBRARY_PATH3, name, subname);
    if (access(path, R_OK) == 0)
        return 0;

    snprintf(path, path_len, "%s/%s.%s.so",HAL_LIBRARY_PATH2, name, subname);
    if (access(path, R_OK) == 0)
        return 0;

    snprintf(path, path_len, "%s/%s.%s.so",HAL_LIBRARY_PATH1, name, subname);
    if (access(path, R_OK) == 0)
        return 0;

    return -ENOENT;
}





static int load(const char *id,const char *path,  const struct hw_module_t **pHmi)
{
    int status = -EINVAL;
    void *handle = NULL;
    struct hw_module_t *hmi = NULL;

    /*
     * load the symbols resolving undefined symbols before
     * dlopen returns. Since RTLD_GLOBAL is not or'd in with
     * RTLD_NOW the external symbols will not be global
     */
    if (strncmp(path, "/system/", 8) == 0) {
        /* If the library is in system partition, no need to check
         * sphal namespace. Open it with dlopen.
         */
        handle = dlopen(path, RTLD_NOW);
    } else {
        //  @   system/core/libvndksupport/linker.c
        handle = android_load_sphal_library(path, RTLD_NOW);
    }



    if (handle == NULL) {
        char const *err_str = dlerror();
        ALOGE("load: module=%s\n%s", path, err_str?err_str:"unknown");
        status = -EINVAL;
        goto done;
    }

    /* Get the address of the struct hal_module_info. */
    //  HMI这是一个约定
    //	@hardware/libhardware/include/hardware/hardware.h:220:#define HAL_MODULE_INFO_SYM_AS_STR  "HMI"
    const char *sym = HAL_MODULE_INFO_SYM_AS_STR;
    hmi = (struct hw_module_t *)dlsym(handle, sym);
    if (hmi == NULL) {
        ALOGE("load: couldn't find symbol %s", sym);
        status = -EINVAL;
        goto done;
    }

    /* Check that the id matches */
    if (strcmp(id, hmi->id) != 0) {
        ALOGE("load: id=%s != hmi->id=%s", id, hmi->id);
        status = -EINVAL;
        goto done;
    }

    hmi->dso = handle;

    /* success */
    status = 0;

    done:
    if (status != 0) {
        hmi = NULL;
        if (handle != NULL) {
            dlclose(handle);
            handle = NULL;
        }
    } else {
        ALOGV("loaded HAL id=%s path=%s hmi=%p handle=%p",id, path, *pHmi, handle);
    }

    *pHmi = hmi;

    return status;
}



/***
 *   @   system/core/libvndksupport/linker.c
 * 
 * 
 *  handle = android_load_sphal_library(path, RTLD_NOW);
 * 
 * 
 * */
void* android_load_sphal_library(const char* name, int flag) {
    // android_namespace_t @  bionic/linker/linker_namespaces.h
    struct android_namespace_t* vendor_namespace = get_vendor_namespace();
    if (vendor_namespace != NULL) {
        const android_dlextinfo dlextinfo = {
            .flags = ANDROID_DLEXT_USE_NAMESPACE,
            .library_namespace = vendor_namespace,
        };
        void* handle = NULL;
        if (android_dlopen_ext != NULL) {
            //  @   bionic/libdl/libdl.cpp
            handle = android_dlopen_ext(name, flag, &dlextinfo);
        }
        if (!handle) {
            ALOGE("Could not load %s from %s namespace: %s.", name, namespace_name, dlerror());
        }
        return handle;
    } else {
        ALOGD("Loading %s from current namespace instead of sphal namespace.", name);
        return dlopen(name, flag);
    }
}


static struct android_namespace_t* get_vendor_namespace() {
    const char* namespace_names[] = {"sphal", "default", NULL};
    static struct android_namespace_t* vendor_namespace = NULL;
    if (vendor_namespace == NULL) {
        int name_idx = 0;
        while (namespace_names[name_idx] != NULL) {

            //  android_get_exported_namespace @    bionic/libdl/libdl.cpp
            if (android_get_exported_namespace != NULL) {
                vendor_namespace = android_get_exported_namespace(namespace_names[name_idx]);
            }
            if (vendor_namespace != NULL) {
                namespace_name = namespace_names[name_idx];
                break;
            }
            name_idx++;

        }
    }
    return vendor_namespace;
}

/**
 * @    bionic/libdl/libdl.cpp
*/
__attribute__((__weak__))
struct android_namespace_t* android_get_exported_namespace(const char* name) {
  return __loader_android_get_exported_namespace(name); //  @   bionic/linker/dlfcn.cpp
}


android_namespace_t* __loader_android_get_exported_namespace(const char* name) {
  return get_exported_namespace(name);
}



// This function finds a namespace exported in ld.config.txt by its name.
// A namespace can be exported by setting .visible property to true.
android_namespace_t* get_exported_namespace(const char* name) {
  if (name == nullptr) {
    return nullptr;
  }
  auto it = g_exported_namespaces.find(std::string(name));
  if (it == g_exported_namespaces.end()) {
    return nullptr;
  }
  return it->second;
}















__attribute__((__weak__))
void* android_dlopen_ext(const char* filename, int flag, const android_dlextinfo* extinfo) {
    /**
     * __builtin_return_address(0)的含义是，得到当前函数返回地址，即此函数被别的函数调用，然后此函数执行完毕后，返回，所谓返回地址就是那时候的地址。
     * __builtin_return_address(1)的含义是，得到当前函数的调用者的返回地址。注意是调用者的返回地址，而不是函数起始地址。
    */
  const void* caller_addr = __builtin_return_address(0);
  return __loader_android_dlopen_ext(filename, flag, extinfo, caller_addr);




void* __loader_android_dlopen_ext(const char* filename,int flags,const android_dlextinfo* extinfo,const void* caller_addr) {
  return dlopen_ext(filename, flags, extinfo, caller_addr);
}



//  @   bionic/linker/dlfcn.cpp
static void* dlopen_ext(const char* filename,int flags const android_dlextinfo* extinfo, const void* caller_addr) {
  ScopedPthreadMutexLocker locker(&g_dl_mutex);
  g_linker_logger.ResetState();
  void* result = do_dlopen(filename, flags, extinfo, caller_addr);
  if (result == nullptr) {
    __bionic_format_dlerror("dlopen failed", linker_get_error_buffer());
    return nullptr;
  }
  return result;
}


//  @   bionic/linker/linker.cpp
void* do_dlopen(const char* name, int flags,const android_dlextinfo* extinfo,const void* caller_addr) {
  std::string trace_prefix = std::string("dlopen: ") + (name == nullptr ? "(nullptr)" : name);
  ScopedTrace trace(trace_prefix.c_str());
  ScopedTrace loading_trace((trace_prefix + " - loading and linking").c_str());
  soinfo* const caller = find_containing_library(caller_addr);
  android_namespace_t* ns = get_caller_namespace(caller);

  LD_LOG(kLogDlopen,
         "dlopen(name=\"%s\", flags=0x%x, extinfo=%s, caller=\"%s\", caller_ns=%s@%p) ...",
         name,
         flags,
         android_dlextinfo_to_string(extinfo).c_str(),
         caller == nullptr ? "(null)" : caller->get_realpath(),
         ns == nullptr ? "(null)" : ns->get_name(),
         ns);

  auto failure_guard = android::base::make_scope_guard([&]() { LD_LOG(kLogDlopen, "... dlopen failed: %s", linker_get_error_buffer()); });

  if ((flags & ~(RTLD_NOW|RTLD_LAZY|RTLD_LOCAL|RTLD_GLOBAL|RTLD_NODELETE|RTLD_NOLOAD)) != 0) {
    DL_ERR("invalid flags to dlopen: %x", flags);
    return nullptr;
  }

  if (extinfo != nullptr) {
    if ((extinfo->flags & ~(ANDROID_DLEXT_VALID_FLAG_BITS)) != 0) {
      DL_ERR("invalid extended flags to android_dlopen_ext: 0x%" PRIx64, extinfo->flags);
      return nullptr;
    }

    if ((extinfo->flags & ANDROID_DLEXT_USE_LIBRARY_FD) == 0 &&
        (extinfo->flags & ANDROID_DLEXT_USE_LIBRARY_FD_OFFSET) != 0) {
      DL_ERR("invalid extended flag combination (ANDROID_DLEXT_USE_LIBRARY_FD_OFFSET without " "ANDROID_DLEXT_USE_LIBRARY_FD): 0x%" PRIx64, extinfo->flags);
      return nullptr;
    }

    if ((extinfo->flags & ANDROID_DLEXT_LOAD_AT_FIXED_ADDRESS) != 0 &&
        (extinfo->flags & (ANDROID_DLEXT_RESERVED_ADDRESS | ANDROID_DLEXT_RESERVED_ADDRESS_HINT)) != 0) {
      DL_ERR("invalid extended flag combination: ANDROID_DLEXT_LOAD_AT_FIXED_ADDRESS is not "  "compatible with ANDROID_DLEXT_RESERVED_ADDRESS/ANDROID_DLEXT_RESERVED_ADDRESS_HINT");
      return nullptr;
    }

    if ((extinfo->flags & ANDROID_DLEXT_USE_NAMESPACE) != 0) {
      if (extinfo->library_namespace == nullptr) {
        DL_ERR("ANDROID_DLEXT_USE_NAMESPACE is set but extinfo->library_namespace is null");
        return nullptr;
      }
      ns = extinfo->library_namespace;
    }
  }

  std::string asan_name_holder;

  const char* translated_name = name;
  if (g_is_asan && translated_name != nullptr && translated_name[0] == '/') {
    char original_path[PATH_MAX];
    if (realpath(name, original_path) != nullptr) {
      asan_name_holder = std::string(kAsanLibDirPrefix) + original_path;
      if (file_exists(asan_name_holder.c_str())) {
        soinfo* si = nullptr;
        if (find_loaded_library_by_realpath(ns, original_path, true, &si)) {
          PRINT("linker_asan dlopen NOT translating \"%s\" -> \"%s\": library already loaded", name,
                asan_name_holder.c_str());
        } else {
          PRINT("linker_asan dlopen translating \"%s\" -> \"%s\"", name, translated_name);
          translated_name = asan_name_holder.c_str();
        }
      }
    }
  }

  ProtectedDataGuard guard;
  soinfo* si = find_library(ns, translated_name, flags, extinfo, caller);
  loading_trace.End();

  if (si != nullptr) {
    void* handle = si->to_handle();
    LD_LOG(kLogDlopen, "... dlopen calling constructors: realpath=\"%s\", soname=\"%s\", handle=%p", si->get_realpath(), si->get_soname(), handle);
    si->call_constructors();
    failure_guard.Disable();
    LD_LOG(kLogDlopen, "... dlopen successful: realpath=\"%s\", soname=\"%s\", handle=%p", si->get_realpath(), si->get_soname(), handle);
    return handle;
  }

  return nullptr;
}


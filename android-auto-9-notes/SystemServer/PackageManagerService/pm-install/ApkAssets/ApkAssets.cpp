

//	@	frameworks/base/libs/androidfw/ApkAssets.cpp
std::unique_ptr<const ApkAssets> ApkAssets::LoadFromFd(unique_fd fd,
                                                       const std::string& friendly_name,
                                                       bool system, bool force_shared_lib) {
	// fd 为 apk文件描述符
	// friendly_name 为 apk路径
	// system 为 false
	// force_shared_lib 为 false
  	return LoadImpl(std::move(fd), friendly_name, nullptr /*idmap_asset*/, nullptr /*loaded_idmap*/, system, force_shared_lib);
}



std::unique_ptr<const ApkAssets> ApkAssets::LoadImpl(
    unique_fd fd, const std::string& path, std::unique_ptr<Asset> idmap_asset,
    std::unique_ptr<const LoadedIdmap> loaded_idmap, bool system, bool load_as_shared_library) {

  ::ZipArchiveHandle unmanaged_handle;
  int32_t result;
  if (fd >= 0) {
    result = :OpenArchiveFd(fd.release(), path.c_str(), &unmanaged_handle, true /*assume_ownership*/);
  } else {
    result = ::OpenArchive(path.c_str(), &unmanaged_handle);
  }

  if (result != 0) {
    LOG(ERROR) << "Failed to open APK '" << path << "' " << ::ErrorCodeString(result);
    return {};
  }

  // Wrap the handle in a unique_ptr so it gets automatically closed.
  std::unique_ptr<ApkAssets> loaded_apk(new ApkAssets(unmanaged_handle, path));

  // Find the resource table.
  // frameworks/base/libs/androidfw/ApkAssets.cpp:40:
  // static const std::string kResourcesArsc("resources.arsc");
  ::ZipString entry_name(kResourcesArsc.c_str());
  ::ZipEntry entry;
  result = ::FindEntry(loaded_apk->zip_handle_.get(), entry_name, &entry);
  if (result != 0) {
    // There is no resources.arsc, so create an empty LoadedArsc and return.
    loaded_apk->loaded_arsc_ = LoadedArsc::CreateEmpty();
    return std::move(loaded_apk);
  }

  // 判别该entry为压缩还是非压缩，kCompressDeflated 为压缩
  if (entry.method == kCompressDeflated) {
    LOG(WARNING) << kResourcesArsc << " in APK '" << path << "' is compressed.";
  }

  // Open the resource table via mmap unless it is compressed. This logic is taken care of by Open.
  // 
  loaded_apk->resources_asset_ = loaded_apk->Open(kResourcesArsc, Asset::AccessMode::ACCESS_BUFFER);
  if (loaded_apk->resources_asset_ == nullptr) {
    LOG(ERROR) << "Failed to open '" << kResourcesArsc << "' in APK '" << path << "'.";
    return {};
  }

  // Must retain ownership of the IDMAP Asset so that all pointers to its mmapped data remain valid.
  //这里idmap_asset为null
  loaded_apk->idmap_asset_ = std::move(idmap_asset);

  const StringPiece data(
      reinterpret_cast<const char*>(loaded_apk->resources_asset_->getBuffer(true /*wordAligned*/)),
      loaded_apk->resources_asset_->getLength());

  // system = false
  // load_as_shared_library = false
  loaded_apk->loaded_arsc_ = LoadedArsc::Load(data, loaded_idmap.get(), system, load_as_shared_library);
  if (loaded_apk->loaded_arsc_ == nullptr) {
    LOG(ERROR) << "Failed to load '" << kResourcesArsc << "' in APK '" << path << "'.";
    return {};
  }

  // Need to force a move for mingw32.
  return std::move(loaded_apk);
}


ApkAssets::ApkAssets(void* unmanaged_handle, const std::string& path)
    : zip_handle_(unmanaged_handle, ::CloseArchive), path_(path) {
}


std::unique_ptr<Asset> ApkAssets::Open(const std::string& path, Asset::AccessMode mode) const {
  CHECK(zip_handle_ != nullptr);

  ::ZipString name(path.c_str());
  ::ZipEntry entry;
  int32_t result = ::FindEntry(zip_handle_.get(), name, &entry);
  if (result != 0) {
    return {};
  }

  if (entry.method == kCompressDeflated) {//压缩解析
    std::unique_ptr<FileMap> map = util::make_unique<FileMap>();
    if (!map->create(path_.c_str(), ::GetFileDescriptor(zip_handle_.get()), entry.offset,
                     entry.compressed_length, true /*readOnly*/)) {
      LOG(ERROR) << "Failed to mmap file '" << path << "' in APK '" << path_ << "'";
      return {};
    }

    std::unique_ptr<Asset> asset =
        Asset::createFromCompressedMap(std::move(map), entry.uncompressed_length, mode);
    if (asset == nullptr) {
      LOG(ERROR) << "Failed to decompress '" << path << "'.";
      return {};
    }
    return asset;
  } else {//非压缩解析
    std::unique_ptr<FileMap> map = util::make_unique<FileMap>();
    if (!map->create(path_.c_str(), ::GetFileDescriptor(zip_handle_.get()), entry.offset,
                     entry.uncompressed_length, true /*readOnly*/)) {
      LOG(ERROR) << "Failed to mmap file '" << path << "' in APK '" << path_ << "'";
      return {};
    }

    std::unique_ptr<Asset> asset = Asset::createFromUncompressedMap(std::move(map), mode);
    if (asset == nullptr) {
      LOG(ERROR) << "Failed to mmap file '" << path << "' in APK '" << path_ << "'";
      return {};
    }
    return asset;
  }
}

// This is never nullptr.
inline const LoadedArsc* ApkAssets::GetLoadedArsc() const {
	return loaded_arsc_.get();
}



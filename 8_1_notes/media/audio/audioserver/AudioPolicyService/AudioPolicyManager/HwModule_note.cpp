//	@frameworks/av/services/audiopolicy/common/managerdefinitions/include/HwModule.h
class HwModuleCollection : public Vector<sp<HwModule> >{}


//	@frameworks/av/services/audiopolicy/common/managerdefinitions/src/HwModule.cpp
HwModule::HwModule(const char *name, uint32_t halVersionMajor, uint32_t halVersionMinor)
    : mName(String8(name)),
      mHandle(AUDIO_MODULE_HANDLE_NONE)
{
    setHalVersion(halVersionMajor, halVersionMinor);
}
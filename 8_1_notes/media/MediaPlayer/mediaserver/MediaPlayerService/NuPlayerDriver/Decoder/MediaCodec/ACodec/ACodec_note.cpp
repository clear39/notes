
//	ColorUtils	@frameworks/av/media/libstagefright/foundation/include/media/stagefright/foundation/ColorUtils.h
//	@frameworks/av/media/libstagefright/include/media/stagefright/CodecBase.h
struct CodecBase : public AHandler, /* static */ ColorUtils {}

//	@frameworks/av/media/libstagefright/foundation/include/media/stagefright/foundation/AHierarchicalStateMachine.h
struct AHierarchicalStateMachine {}

//	@frameworks/av/media/libstagefright/include/media/stagefright/ACodec.h
struct ACodec : public AHierarchicalStateMachine, public CodecBase {}



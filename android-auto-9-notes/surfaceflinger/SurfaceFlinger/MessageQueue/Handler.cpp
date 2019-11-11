

//  @   frameworks/native/services/surfaceflinger/MessageQueue.h

class Handler : public MessageHandler {
    enum { eventMaskInvalidate = 0x1, eventMaskRefresh = 0x2, eventMaskTransaction = 0x4 };
    MessageQueue& mQueue;
    int32_t mEventMask;

public:
    explicit Handler(MessageQueue& queue) : mQueue(queue), mEventMask(0) {}
    virtual void handleMessage(const Message& message);
    void dispatchRefresh();
    void dispatchInvalidate();
};
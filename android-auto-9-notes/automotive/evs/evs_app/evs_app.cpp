//  @/work/workcodes/aosp-p9.x-auto-ga/packages/services/Car/evs/app/evs_app.cpp


// Main entry point
int main(int argc, char** argv)
{
    ALOGI("EVS app starting\n");

    // Set up default behavior, then check for command line options
    bool useVehicleHal = true;
    bool printHelp = false;
    const char* evsServiceName = "default";
    for (int i=1; i< argc; i++) {
        if (strcmp(argv[i], "--test") == 0) {
            useVehicleHal = false;
        } else if (strcmp(argv[i], "--hw") == 0) {
            evsServiceName = "EvsEnumeratorHw";
        } else if (strcmp(argv[i], "--mock") == 0) {
            evsServiceName = "EvsEnumeratorHw-Mock";
        } else if (strcmp(argv[i], "--help") == 0) {
            printHelp = true;
        } else {
            printf("Ignoring unrecognized command line arg '%s'\n", argv[i]);
            printHelp = true;
        }
    }
    if (printHelp) {
        printf("Options include:\n");
        printf("  --test   Do not talk to Vehicle Hal, but simulate 'reverse' instead\n");
        printf("  --hw     Bypass EvsManager by connecting directly to EvsEnumeratorHw\n");
        printf("  --mock   Connect directly to EvsEnumeratorHw-Mock\n");
    }

    // Load our configuration information
    ConfigManager config;
    if (!config.initialize("/system/etc/automotive/evs/config.json")) {
        ALOGE("Missing or improper configuration for the EVS application.  Exiting.");
        return 1;
    }

    // Set thread pool size to one to avoid concurrent events from the HAL.
    // This pool will handle the EvsCameraStream callbacks.
    // Note:  This _will_ run in parallel with the EvsListener run() loop below which
    // runs the application logic that reacts to the async events.
    configureRpcThreadpool(1, false /* callerWillJoin */);

    // Construct our async helper object
    sp<EvsVehicleListener> pEvsListener = new EvsVehicleListener();

    // Get the EVS manager service
    ALOGI("Acquiring EVS Enumerator");
    android::sp<IEvsEnumerator> pEvs = IEvsEnumerator::getService(evsServiceName);
    if (pEvs.get() == nullptr) {
        ALOGE("getService(%s) returned NULL.  Exiting.", evsServiceName);
        return 1;
    }

    // Request exclusive access to the EVS display
    ALOGI("Acquiring EVS Display");
    android::sp <IEvsDisplay> pDisplay;
    pDisplay = pEvs->openDisplay();
    if (pDisplay.get() == nullptr) {
        ALOGE("EVS Display unavailable.  Exiting.");
        return 1;
    }

    // Connect to the Vehicle HAL so we can monitor state
    /**
     * 这里 useVehicleHal 默认为true，启动时 设置参数 --test
     * 车辆信息服务监测，主要监测 VehicleProperty::GEAR_SELECTION 和 VehicleProperty::TURN_SIGNAL_STATE
     */
    sp<IVehicle> pVnet;
    if (useVehicleHal) {
        ALOGI("Connecting to Vehicle HAL");
        pVnet = IVehicle::getService();
        if (pVnet.get() == nullptr) {
            ALOGE("Vehicle HAL getService returned NULL.  Exiting.");
            return 1;
        } else {
            // Register for vehicle state change callbacks we care about
            // Changes in these values are what will trigger a reconfiguration of the EVS pipeline
            if (!subscribeToVHal(pVnet, pEvsListener, VehicleProperty::GEAR_SELECTION)) {
                ALOGE("Without gear notification, we can't support EVS.  Exiting.");
                return 1;
            }
            if (!subscribeToVHal(pVnet, pEvsListener, VehicleProperty::TURN_SIGNAL_STATE)) {
                ALOGW("Didn't get turn signal notificaitons, so we'll ignore those.");
            }
        }
    } else {
        ALOGW("Test mode selected, so not talking to Vehicle HAL");
    }

    // Configure ourselves for the current vehicle state at startup
    ALOGI("Constructing state controller");
    EvsStateControl *pStateController = new EvsStateControl(pVnet, pEvs, pDisplay, config);
    if (!pStateController->startUpdateLoop()) {
        ALOGE("Initial configuration failed.  Exiting.");
        return 1;
    }

    // Run forever, reacting to events as necessary
    ALOGI("Entering running state");
    /**
     * 这里 EvsVehicleListener 进入while循环，并且等待，直到 VehicleProperty::GEAR_SELECTION 和 VehicleProperty::TURN_SIGNAL_STATE 属性值更改
     * 之后，会调用 EvsStateControl 的 postCommand 方法触发消息
     */
    pEvsListener->run(pStateController);

    // In normal operation, we expect to run forever, but in some error conditions we'll quit.
    // One known example is if another process preempts our registration for our service name.
    ALOGE("EVS Listener stopped.  Exiting.");

    return 0;
}


bool EvsStateControl::startUpdateLoop() {
    // Create the thread and report success if it gets started
    mRenderThread = std::thread([this](){ updateLoop(); });
    /**
     * joinable 执行成功返回true，否则返回false
     */
    return mRenderThread.joinable();
}

void EvsStateControl::postCommand(const Command& cmd) {
    // Push the command onto the queue watched by updateLoop
    mLock.lock();
    mCommandQueue.push(cmd);
    mLock.unlock();

    // Send a signal to wake updateLoop in case it is asleep
    mWakeSignal.notify_all();
}



void EvsStateControl::updateLoop() {
    ALOGD("Starting EvsStateControl update loop");

    bool run = true;
    while (run) {
        // Process incoming commands
        {
            /**
             * 这里是由 postCommand
             */
            std::lock_guard <std::mutex> lock(mLock);
            while (!mCommandQueue.empty()) {
                const Command& cmd = mCommandQueue.front();
                switch (cmd.operation) {
                case Op::EXIT:
                    run = false;
                    break;
                case Op::CHECK_VEHICLE_STATE:
                    // Just running selectStateForCurrentConditions below will take care of this
                    break;
                case Op::TOUCH_EVENT:
                    // TODO:  Implement this given the x/y location of the touch event
                    // Ignore for now
                    break;
                }
                mCommandQueue.pop();
            }
        }

        // Review vehicle state and choose an appropriate renderer
        if (!selectStateForCurrentConditions()) {
            ALOGE("selectStateForCurrentConditions failed so we're going to die");
            break;
        }

        // If we have an active renderer, give it a chance to draw
        if (mCurrentRenderer) {
            // Get the output buffer we'll use to display the imagery
            BufferDesc tgtBuffer = {};
            mDisplay->getTargetBuffer([&tgtBuffer](const BufferDesc& buff) {
                                          tgtBuffer = buff;
                                      }
            );

            if (tgtBuffer.memHandle == nullptr) {
                ALOGE("Didn't get requested output buffer -- skipping this frame.");
            } else {
                // Generate our output image
                if (!mCurrentRenderer->drawFrame(tgtBuffer)) {
                    // If drawing failed, we want to exit quickly so an app restart can happen
                    run = false;
                }

                // Send the finished image back for display
                mDisplay->returnTargetBufferForDisplay(tgtBuffer);
            }
        } else {
            // No active renderer, so sleep until somebody wakes us with another command
            std::unique_lock<std::mutex> lock(mLock);
            mWakeSignal.wait(lock);
        }
    }

    ALOGW("EvsStateControl update loop ending");

    // TODO:  Fix it so we can exit cleanly from the main thread instead
    printf("Shutting down app due to state control loop ending\n");
    ALOGE("KILLING THE APP FROM THE EvsStateControl LOOP ON DRAW FAILURE!!!");
    exit(1);
}

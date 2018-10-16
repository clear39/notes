
//	@system/core/logd/LogBuffer.cpp

//	LogBuffer继承LogBufferInterface

//	LogTimeEntry@system/core/logd/LogTimes.h:32
//	LastLogTimes@system/core/logd/LogTimes.h:144:typedef std::list<LogTimeEntry*> LastLogTimes;
LogBuffer::LogBuffer(LastLogTimes* times)
	// android_log_clockid @system/core/liblog/properties.c:367
    : monotonic(android_log_clockid() == CLOCK_MONOTONIC), mTimes(*times) {
    pthread_rwlock_init(&mLogElementsLock, nullptr);

    log_id_for_each(i) {
        lastLoggedElements[i] = nullptr;	//LogBufferElement* lastLoggedElements[LOG_ID_MAX];
        droppedElements[i] = nullptr;		//LogBufferElement* droppedElements[LOG_ID_MAX];
    }

    init();
}

/*
 * Timestamp state generally remains constant, but can change at any time
 * to handle developer requirements.
 */
LIBLOG_ABI_PUBLIC clockid_t android_log_clockid() {
  static struct cache2_char clockid = {
    PTHREAD_MUTEX_INITIALIZER, 0,
    "persist.logd.timestamp",  { { NULL, -1 }, '\0' },
    "ro.logd.timestamp",       { { NULL, -1 }, '\0' },
    evaluate_persist_ro
  };

  return (tolower(do_cache2_char(&clockid)) == 'm') ? CLOCK_MONOTONIC : CLOCK_REALTIME;
}



void LogBuffer::init() {
    log_id_for_each(i) {
        mLastSet[i] = false;
        //	typedef std::list<LogBufferElement*> LogBufferElementCollection;
        mLast[i] = mLogElements.begin(); //LogBufferElementCollection mLogElements;

        if (setSize(i, __android_logger_get_buffer_size(i))) {
            setSize(i, LOG_BUFFER_MIN_SIZE);
        }
    }

    bool lastMonotonic = monotonic;
    monotonic = android_log_clockid() == CLOCK_MONOTONIC;
    if (lastMonotonic != monotonic) {
        //
        // Fixup all timestamps, may not be 100% accurate, but better than
        // throwing what we have away when we get 'surprised' by a change.
        // In-place element fixup so no need to check reader-lock. Entries
        // should already be in timestamp order, but we could end up with a
        // few out-of-order entries if new monotonics come in before we
        // are notified of the reinit change in status. A Typical example would
        // be:
        //  --------- beginning of system
        //      10.494082   184   201 D Cryptfs : Just triggered post_fs_data
        //  --------- beginning of kernel
        //       0.000000     0     0 I         : Initializing cgroup subsys
        // as the act of mounting /data would trigger persist.logd.timestamp to
        // be corrected. 1/30 corner case YMMV.
        //
        rdlock();
        LogBufferElementCollection::iterator it = mLogElements.begin();
        while ((it != mLogElements.end())) {
            LogBufferElement* e = *it;
            if (monotonic) {
                if (!android::isMonotonic(e->mRealTime)) {
                    LogKlog::convertRealToMonotonic(e->mRealTime);
                    if ((e->mRealTime.tv_nsec % 1000) == 0) {
                        e->mRealTime.tv_nsec++;
                    }
                }
            } else {
                if (android::isMonotonic(e->mRealTime)) {
                    LogKlog::convertMonotonicToReal(e->mRealTime);
                    if ((e->mRealTime.tv_nsec % 1000) == 0) {
                        e->mRealTime.tv_nsec++;
                    }
                }
            }
            ++it;
        }
        unlock();
    }

    // We may have been triggered by a SIGHUP. Release any sleeping reader
    // threads to dump their current content.
    //
    // NB: this is _not_ performed in the context of a SIGHUP, it is
    // performed during startup, and in context of reinit administrative thread
    LogTimeEntry::wrlock();

    LastLogTimes::iterator times = mTimes.begin();
    while (times != mTimes.end()) {
        LogTimeEntry* entry = (*times);
        if (entry->owned_Locked()) {
            entry->triggerReader_Locked();
        }
        times++;
    }

    LogTimeEntry::unlock();
}




int LogBuffer::log(log_id_t log_id, log_time realtime, uid_t uid, pid_t pid,pid_t tid, const char* msg, unsigned short len) {
    if ((log_id >= LOG_ID_MAX) || (log_id < 0)) {
        return -EINVAL;
    }

    // Slip the time by 1 nsec if the incoming lands on xxxxxx000 ns.
    // This prevents any chance that an outside source can request an
    // exact entry with time specified in ms or us precision.
    if ((realtime.tv_nsec % 1000) == 0) ++realtime.tv_nsec;

    LogBufferElement* elem = new LogBufferElement(log_id, realtime, uid, pid, tid, msg, len);
    if (log_id != LOG_ID_SECURITY) {
        int prio = ANDROID_LOG_INFO;
        const char* tag = nullptr;
        if (log_id == LOG_ID_EVENTS) {
            tag = tagToName(elem->getTag());
        } else {
            prio = *msg;
            tag = msg + 1;
        }
        if (!__android_log_is_loggable(prio, tag, ANDROID_LOG_VERBOSE)) {
            // Log traffic received to total
            wrlock();
            stats.addTotal(elem);
            unlock();
            delete elem;
            return -EACCES;
        }
    }

    wrlock();
    LogBufferElement* currentLast = lastLoggedElements[log_id];
    if (currentLast) {
        LogBufferElement* dropped = droppedElements[log_id];
        unsigned short count = dropped ? dropped->getDropped() : 0;
        //
        // State Init
        //     incoming:
        //         dropped = nullptr
        //         currentLast = nullptr;
        //         elem = incoming message
        //     outgoing:
        //         dropped = nullptr -> State 0
        //         currentLast = copy of elem
        //         log elem
        // State 0
        //     incoming:
        //         count = 0
        //         dropped = nullptr
        //         currentLast = copy of last message
        //         elem = incoming message
        //     outgoing: if match != DIFFERENT
        //         dropped = copy of first identical message -> State 1
        //         currentLast = reference to elem
        //     break: if match == DIFFERENT
        //         dropped = nullptr -> State 0
        //         delete copy of last message (incoming currentLast)
        //         currentLast = copy of elem
        //         log elem
        // State 1
        //     incoming:
        //         count = 0
        //         dropped = copy of first identical message
        //         currentLast = reference to last held-back incoming
        //                       message
        //         elem = incoming message
        //     outgoing: if match == SAME
        //         delete copy of first identical message (dropped)
        //         dropped = reference to last held-back incoming
        //                   message set to chatty count of 1 -> State 2
        //         currentLast = reference to elem
        //     outgoing: if match == SAME_LIBLOG
        //         dropped = copy of first identical message -> State 1
        //         take sum of currentLast and elem
        //         if sum overflows:
        //             log currentLast
        //             currentLast = reference to elem
        //         else
        //             delete currentLast
        //             currentLast = reference to elem, sum liblog.
        //     break: if match == DIFFERENT
        //         delete dropped
        //         dropped = nullptr -> State 0
        //         log reference to last held-back (currentLast)
        //         currentLast = copy of elem
        //         log elem
        // State 2
        //     incoming:
        //         count = chatty count
        //         dropped = chatty message holding count
        //         currentLast = reference to last held-back incoming
        //                       message.
        //         dropped = chatty message holding count
        //         elem = incoming message
        //     outgoing: if match != DIFFERENT
        //         delete chatty message holding count
        //         dropped = reference to last held-back incoming
        //                   message, set to chatty count + 1
        //         currentLast = reference to elem
        //     break: if match == DIFFERENT
        //         log dropped (chatty message)
        //         dropped = nullptr -> State 0
        //         log reference to last held-back (currentLast)
        //         currentLast = copy of elem
        //         log elem
        //
        enum match_type match = identical(elem, currentLast);
        if (match != DIFFERENT) {
            if (dropped) {
                // Sum up liblog tag messages?
                if ((count == 0) /* at Pass 1 */ && (match == SAME_LIBLOG)) {
                    android_log_event_int_t* event =
                        reinterpret_cast<android_log_event_int_t*>(
                            const_cast<char*>(currentLast->getMsg()));
                    //
                    // To unit test, differentiate with something like:
                    //    event->header.tag = htole32(CHATTY_LOG_TAG);
                    // here, then instead of delete currentLast below,
                    // log(currentLast) to see the incremental sums form.
                    //
                    uint32_t swab = event->payload.data;
                    unsigned long long total = htole32(swab);
                    event = reinterpret_cast<android_log_event_int_t*>(
                        const_cast<char*>(elem->getMsg()));
                    swab = event->payload.data;

                    lastLoggedElements[LOG_ID_EVENTS] = elem;
                    total += htole32(swab);
                    // check for overflow
                    if (total >= UINT32_MAX) {
                        log(currentLast);
                        unlock();
                        return len;
                    }
                    stats.addTotal(currentLast);
                    delete currentLast;
                    swab = total;
                    event->payload.data = htole32(swab);
                    unlock();
                    return len;
                }
                if (count == USHRT_MAX) {
                    log(dropped);
                    count = 1;
                } else {
                    delete dropped;
                    ++count;
                }
            }
            if (count) {
                stats.addTotal(currentLast);
                currentLast->setDropped(count);
            }
            droppedElements[log_id] = currentLast;
            lastLoggedElements[log_id] = elem;
            unlock();
            return len;
        }
        if (dropped) {         // State 1 or 2
            if (count) {       // State 2
                log(dropped);  // report chatty
            } else {           // State 1
                delete dropped;
            }
            droppedElements[log_id] = nullptr;
            log(currentLast);  // report last message in the series
        } else {               // State 0
            delete currentLast;
        }
    }
    lastLoggedElements[log_id] = new LogBufferElement(*elem);

    log(elem);
    unlock();

    return len;
}


// assumes LogBuffer::wrlock() held, owns elem, look after garbage collection
void LogBuffer::log(LogBufferElement* elem) {
    // cap on how far back we will sort in-place, otherwise append
    static uint32_t too_far_back = 5;  // five seconds
    // Insert elements in time sorted order if possible
    //  NB: if end is region locked, place element at end of list
    LogBufferElementCollection::iterator it = mLogElements.end();
    LogBufferElementCollection::iterator last = it;
    if (__predict_true(it != mLogElements.begin())) --it;
    if (__predict_false(it == mLogElements.begin()) ||
        __predict_true((*it)->getRealTime() <= elem->getRealTime()) ||
        __predict_false((((*it)->getRealTime().tv_sec - too_far_back) >
                         elem->getRealTime().tv_sec) &&
                        (elem->getLogId() != LOG_ID_KERNEL) &&
                        ((*it)->getLogId() != LOG_ID_KERNEL))) {
        mLogElements.push_back(elem);
    } else {
        log_time end = log_time::EPOCH;
        bool end_set = false;
        bool end_always = false;

        LogTimeEntry::rdlock();

        LastLogTimes::iterator times = mTimes.begin();
        while (times != mTimes.end()) {
            LogTimeEntry* entry = (*times);
            if (entry->owned_Locked()) {
                if (!entry->mNonBlock) {
                    end_always = true;
                    break;
                }
                // it passing mEnd is blocked by the following checks.
                if (!end_set || (end <= entry->mEnd)) {
                    end = entry->mEnd;
                    end_set = true;
                }
            }
            times++;
        }

        if (end_always || (end_set && (end > (*it)->getRealTime()))) {
            mLogElements.push_back(elem);
        } else {
            // should be short as timestamps are localized near end()
            do {
                last = it;
                if (__predict_false(it == mLogElements.begin())) {
                    break;
                }
                --it;
            } while (((*it)->getRealTime() > elem->getRealTime()) &&
                     (!end_set || (end <= (*it)->getRealTime())));
            mLogElements.insert(last, elem);
        }
        LogTimeEntry::unlock();
    }

    stats.add(elem);
    maybePrune(elem->getLogId());
}


// Prune at most 10% of the log entries or maxPrune, whichever is less.
//
// LogBuffer::wrlock() must be held when this function is called.
void LogBuffer::maybePrune(log_id_t id) {
    size_t sizes = stats.sizes(id);
    unsigned long maxSize = log_buffer_size(id);
    if (sizes > maxSize) {
        size_t sizeOver = sizes - ((maxSize * 9) / 10);
        size_t elements = stats.realElements(id);
        size_t minElements = elements / 100;
        if (minElements < minPrune) {
            minElements = minPrune;
        }
        unsigned long pruneRows = elements * sizeOver / sizes;
        if (pruneRows < minElements) {
            pruneRows = minElements;
        }
        if (pruneRows > maxPrune) {
            pruneRows = maxPrune;
        }
        prune(id, pruneRows);
    }
}


bool LogBuffer::prune(log_id_t id, unsigned long pruneRows, uid_t caller_uid) {
    LogTimeEntry* oldest = nullptr;
    bool busy = false;
    bool clearAll = pruneRows == ULONG_MAX;

    LogTimeEntry::rdlock();

    // Region locked?
    LastLogTimes::iterator times = mTimes.begin();
    while (times != mTimes.end()) {
        LogTimeEntry* entry = (*times);
        if (entry->owned_Locked() && entry->isWatching(id) &&
            (!oldest || (oldest->mStart > entry->mStart) ||
             ((oldest->mStart == entry->mStart) &&
              (entry->mTimeout.tv_sec || entry->mTimeout.tv_nsec)))) {
            oldest = entry;
        }
        times++;
    }
    log_time watermark(log_time::tv_sec_max, log_time::tv_nsec_max);
    if (oldest) watermark = oldest->mStart - pruneMargin;

    LogBufferElementCollection::iterator it;

    if (__predict_false(caller_uid != AID_ROOT)) {  // unlikely
        // Only here if clear all request from non system source, so chatty
        // filter logistics is not required.
        it = mLastSet[id] ? mLast[id] : mLogElements.begin();
        while (it != mLogElements.end()) {
            LogBufferElement* element = *it;

            if ((element->getLogId() != id) ||
                (element->getUid() != caller_uid)) {
                ++it;
                continue;
            }

            if (!mLastSet[id] || ((*mLast[id])->getLogId() != id)) {
                mLast[id] = it;
                mLastSet[id] = true;
            }

            if (oldest && (watermark <= element->getRealTime())) {
                busy = isBusy(watermark);
                if (busy) kickMe(oldest, id, pruneRows);
                break;
            }

            it = erase(it);
            if (--pruneRows == 0) {
                break;
            }
        }
        LogTimeEntry::unlock();
        return busy;
    }

    // prune by worst offenders; by blacklist, UID, and by PID of system UID
    bool hasBlacklist = (id != LOG_ID_SECURITY) && mPrune.naughty();
    while (!clearAll && (pruneRows > 0)) {
        // recalculate the worst offender on every batched pass
        int worst = -1;  // not valid for getUid() or getKey()
        size_t worst_sizes = 0;
        size_t second_worst_sizes = 0;
        pid_t worstPid = 0;  // POSIX guarantees PID != 0

        if (worstUidEnabledForLogid(id) && mPrune.worstUidEnabled()) {
            // Calculate threshold as 12.5% of available storage
            size_t threshold = log_buffer_size(id) / 8;

            if ((id == LOG_ID_EVENTS) || (id == LOG_ID_SECURITY)) {
                stats.sortTags(AID_ROOT, (pid_t)0, 2, id)
                    .findWorst(worst, worst_sizes, second_worst_sizes,
                               threshold);
                // per-pid filter for AID_SYSTEM sources is too complex
            } else {
                stats.sort(AID_ROOT, (pid_t)0, 2, id)
                    .findWorst(worst, worst_sizes, second_worst_sizes,
                               threshold);

                if ((worst == AID_SYSTEM) && mPrune.worstPidOfSystemEnabled()) {
                    stats.sortPids(worst, (pid_t)0, 2, id)
                        .findWorst(worstPid, worst_sizes, second_worst_sizes);
                }
            }
        }

        // skip if we have neither worst nor naughty filters
        if ((worst == -1) && !hasBlacklist) {
            break;
        }

        bool kick = false;
        bool leading = true;
        it = mLastSet[id] ? mLast[id] : mLogElements.begin();
        // Perform at least one mandatory garbage collection cycle in following
        // - clear leading chatty tags
        // - coalesce chatty tags
        // - check age-out of preserved logs
        bool gc = pruneRows <= 1;
        if (!gc && (worst != -1)) {
            {  // begin scope for worst found iterator
                LogBufferIteratorMap::iterator found =
                    mLastWorst[id].find(worst);
                if ((found != mLastWorst[id].end()) &&
                    (found->second != mLogElements.end())) {
                    leading = false;
                    it = found->second;
                }
            }
            if (worstPid) {  // begin scope for pid worst found iterator
                // FYI: worstPid only set if !LOG_ID_EVENTS and
                //      !LOG_ID_SECURITY, not going to make that assumption ...
                LogBufferPidIteratorMap::iterator found =
                    mLastWorstPidOfSystem[id].find(worstPid);
                if ((found != mLastWorstPidOfSystem[id].end()) &&
                    (found->second != mLogElements.end())) {
                    leading = false;
                    it = found->second;
                }
            }
        }
        static const timespec too_old = { EXPIRE_HOUR_THRESHOLD * 60 * 60, 0 };
        LogBufferElementCollection::iterator lastt;
        lastt = mLogElements.end();
        --lastt;
        LogBufferElementLast last;
        while (it != mLogElements.end()) {
            LogBufferElement* element = *it;

            if (oldest && (watermark <= element->getRealTime())) {
                busy = isBusy(watermark);
                // Do not let chatty eliding trigger any reader mitigation
                break;
            }

            if (element->getLogId() != id) {
                ++it;
                continue;
            }
            // below this point element->getLogId() == id

            if (leading && (!mLastSet[id] || ((*mLast[id])->getLogId() != id))) {
                mLast[id] = it;
                mLastSet[id] = true;
            }

            unsigned short dropped = element->getDropped();

            // remove any leading drops
            if (leading && dropped) {
                it = erase(it);
                continue;
            }

            if (dropped && last.coalesce(element, dropped)) {
                it = erase(it, true);
                continue;
            }

            int key = ((id == LOG_ID_EVENTS) || (id == LOG_ID_SECURITY))
                          ? element->getTag()
                          : element->getUid();

            if (hasBlacklist && mPrune.naughty(element)) {
                last.clear(element);
                it = erase(it);
                if (dropped) {
                    continue;
                }

                pruneRows--;
                if (pruneRows == 0) {
                    break;
                }

                if (key == worst) {
                    kick = true;
                    if (worst_sizes < second_worst_sizes) {
                        break;
                    }
                    worst_sizes -= element->getMsgLen();
                }
                continue;
            }

            if ((element->getRealTime() < ((*lastt)->getRealTime() - too_old)) ||
                (element->getRealTime() > (*lastt)->getRealTime())) {
                break;
            }

            if (dropped) {
                last.add(element);
                if (worstPid &&
                    ((!gc && (element->getPid() == worstPid)) ||
                     (mLastWorstPidOfSystem[id].find(element->getPid()) ==
                      mLastWorstPidOfSystem[id].end()))) {
                    // element->getUid() may not be AID_SYSTEM, next best
                    // watermark if current one empty. id is not LOG_ID_EVENTS
                    // or LOG_ID_SECURITY because of worstPid check.
                    mLastWorstPidOfSystem[id][element->getPid()] = it;
                }
                if ((!gc && !worstPid && (key == worst)) ||
                    (mLastWorst[id].find(key) == mLastWorst[id].end())) {
                    mLastWorst[id][key] = it;
                }
                ++it;
                continue;
            }

            if ((key != worst) ||
                (worstPid && (element->getPid() != worstPid))) {
                leading = false;
                last.clear(element);
                ++it;
                continue;
            }
            // key == worst below here
            // If worstPid set, then element->getPid() == worstPid below here

            pruneRows--;
            if (pruneRows == 0) {
                break;
            }

            kick = true;

            unsigned short len = element->getMsgLen();

            // do not create any leading drops
            if (leading) {
                it = erase(it);
            } else {
                stats.drop(element);
                element->setDropped(1);
                if (last.coalesce(element, 1)) {
                    it = erase(it, true);
                } else {
                    last.add(element);
                    if (worstPid &&
                        (!gc || (mLastWorstPidOfSystem[id].find(worstPid) ==
                                 mLastWorstPidOfSystem[id].end()))) {
                        // element->getUid() may not be AID_SYSTEM, next best
                        // watermark if current one empty. id is not
                        // LOG_ID_EVENTS or LOG_ID_SECURITY because of worstPid.
                        mLastWorstPidOfSystem[id][worstPid] = it;
                    }
                    if ((!gc && !worstPid) ||
                        (mLastWorst[id].find(worst) == mLastWorst[id].end())) {
                        mLastWorst[id][worst] = it;
                    }
                    ++it;
                }
            }
            if (worst_sizes < second_worst_sizes) {
                break;
            }
            worst_sizes -= len;
        }
        last.clear();

        if (!kick || !mPrune.worstUidEnabled()) {
            break;  // the following loop will ask bad clients to skip/drop
        }
    }

    bool whitelist = false;
    bool hasWhitelist = (id != LOG_ID_SECURITY) && mPrune.nice() && !clearAll;
    it = mLastSet[id] ? mLast[id] : mLogElements.begin();
    while ((pruneRows > 0) && (it != mLogElements.end())) {
        LogBufferElement* element = *it;

        if (element->getLogId() != id) {
            it++;
            continue;
        }

        if (!mLastSet[id] || ((*mLast[id])->getLogId() != id)) {
            mLast[id] = it;
            mLastSet[id] = true;
        }

        if (oldest && (watermark <= element->getRealTime())) {
            busy = isBusy(watermark);
            if (!whitelist && busy) kickMe(oldest, id, pruneRows);
            break;
        }

        if (hasWhitelist && !element->getDropped() && mPrune.nice(element)) {
            // WhiteListed
            whitelist = true;
            it++;
            continue;
        }

        it = erase(it);
        pruneRows--;
    }

    // Do not save the whitelist if we are reader range limited
    if (whitelist && (pruneRows > 0)) {
        it = mLastSet[id] ? mLast[id] : mLogElements.begin();
        while ((it != mLogElements.end()) && (pruneRows > 0)) {
            LogBufferElement* element = *it;

            if (element->getLogId() != id) {
                ++it;
                continue;
            }

            if (!mLastSet[id] || ((*mLast[id])->getLogId() != id)) {
                mLast[id] = it;
                mLastSet[id] = true;
            }

            if (oldest && (watermark <= element->getRealTime())) {
                busy = isBusy(watermark);
                if (busy) kickMe(oldest, id, pruneRows);
                break;
            }

            it = erase(it);
            pruneRows--;
        }
    }

    LogTimeEntry::unlock();

    return (pruneRows > 0) && busy;
}




/**
 * Access to the system diagnostic(诊断的) event record.  System diagnostic events are
 * used to record certain(某些) system-level events (such as garbage(垃圾) collection,
 * activity manager state, system watchdogs, and other low level activity),
 * which may be automatically collected and analyzed during system development.
 *
 * <p>This is <b>not</b> the main "logcat" debugging log ({@link android.util.Log})!
 * These diagnostic events are for system integrators(积分器), not application authors.
 *
 * <p>Events use integer tag codes corresponding(相应的) to /system/etc/event-log-tags.
 * They carry(携带) a payload of one or more int, long, or String values.  The
 * event-log-tags file defines the payload contents for each type code.
 */


//java层api	frameworks/base/core/java/android/util/EventLog.java
{
	/**
	* Get the name associated with an event type tag code.
	* @param tag code to look up
	* @return the name of the tag, or null if no tag has that number
	*/
	public static String getTagName(int tag) {
		readTagsFile();
		return sTagNames.get(tag);
	}

	/**
	* Get the event type tag code associated with an event name.
	* @param name of event to look up
	* @return the tag code, or -1 if no tag has that name
	*/
	public static int getTagCode(String name) {
		readTagsFile();
		Integer code = sTagCodes.get(name);
		return code != null ? code : -1;
	}

	/**
	* Read TAGS_FILE, populating sTagCodes and sTagNames, if not already done.
	*/
	private static synchronized void readTagsFile() {
		if (sTagCodes != null && sTagNames != null) return;

		sTagCodes = new HashMap<String, Integer>();
		sTagNames = new HashMap<Integer, String>();

		/**
		"^\\s*(#.*)?$"

		^ 	为匹配输入字符串的开始位置
		\s	
		*	字符可以不出现，也可以出现一次或者多次
		? 问号代表前面的字符最多只可以出现一次（0次、或1次）
		*/
		Pattern comment = Pattern.compile(COMMENT_PATTERN);//COMMENT_PATTERN = "^\\s*(#.*)?$";
		Pattern tag = Pattern.compile(TAG_PATTERN);//TAG_PATTERN = "^\\s*(\\d+)\\s+(\\w+)\\s*(\\(.*\\))?\\s*$";
		BufferedReader reader = null;
		String line;

		try {
		    reader = new BufferedReader(new FileReader(TAGS_FILE), 256);//	TAGS_FILE = "/system/etc/event-log-tags";
		    while ((line = reader.readLine()) != null) {
			if (comment.matcher(line).matches()) continue;

			Matcher m = tag.matcher(line);
			if (!m.matches()) {
			    Log.wtf(TAG, "Bad entry in " + TAGS_FILE + ": " + line);
			    continue;
			}

			try {
			    int num = Integer.parseInt(m.group(1));
			    String name = m.group(2);
			    sTagCodes.put(name, num);
			    sTagNames.put(num, name);
			} catch (NumberFormatException e) {
			    Log.wtf(TAG, "Error in " + TAGS_FILE + ": " + line, e);
			}
		    }
		} catch (IOException e) {
		    Log.wtf(TAG, "Error reading " + TAGS_FILE, e);
		    // Leave the maps existing but unpopulated
		} finally {
		    try { if (reader != null) reader.close(); } catch (IOException e) {}
		}
	}
}

{
	//本地代码	frameworks/base/core/jni/android_util_EventLog.cpp


	//	system/core/include/log/log.h:548
	#define android_btWriteLog(tag, type, payload, len) __android_log_btwrite(tag, type, payload, len)

	//	system/core/liblog/logd_write.c:389
	/*
	 * Like __android_log_bwrite, but takes the type as well.  Doesn't work
	 * for the general case where we're generating lists of stuff, but very
	 * handy if we just want to dump an integer into the log.
	 */
	int __android_log_btwrite(int32_t tag, char type, const void *payload,size_t len)
	{
	    struct iovec vec[3];

	    vec[0].iov_base = &tag;
	    vec[0].iov_len = sizeof(tag);
	    vec[1].iov_base = &type;
	    vec[1].iov_len = sizeof(type);
	    vec[2].iov_base = (void*)payload;
	    vec[2].iov_len = len;

	    return write_to_log(LOG_ID_EVENTS, vec, 3);
	}

	//	static int (*write_to_log)(log_id_t, struct iovec *vec, size_t nr) = __write_to_log_init;
	static int __write_to_log_init(log_id_t log_id, struct iovec *vec, size_t nr)
	{
	#ifdef HAVE_PTHREADS
	    pthread_mutex_lock(&log_init_lock);
	#endif

	    if (write_to_log == __write_to_log_init) {
		log_fds[LOG_ID_MAIN] = log_open("/dev/"LOGGER_LOG_MAIN, O_WRONLY);
		log_fds[LOG_ID_RADIO] = log_open("/dev/"LOGGER_LOG_RADIO, O_WRONLY);
		log_fds[LOG_ID_EVENTS] = log_open("/dev/"LOGGER_LOG_EVENTS, O_WRONLY);
		log_fds[LOG_ID_SYSTEM] = log_open("/dev/"LOGGER_LOG_SYSTEM, O_WRONLY);

		write_to_log = __write_to_log_kernel;

		if (log_fds[LOG_ID_MAIN] < 0 || log_fds[LOG_ID_RADIO] < 0 || log_fds[LOG_ID_EVENTS] < 0) {
		    log_close(log_fds[LOG_ID_MAIN]);
		    log_close(log_fds[LOG_ID_RADIO]);
		    log_close(log_fds[LOG_ID_EVENTS]);
		    log_fds[LOG_ID_MAIN] = -1;
		    log_fds[LOG_ID_RADIO] = -1;
		    log_fds[LOG_ID_EVENTS] = -1;
		    write_to_log = __write_to_log_null;
		}

		if (log_fds[LOG_ID_SYSTEM] < 0) {
		    log_fds[LOG_ID_SYSTEM] = log_fds[LOG_ID_MAIN];
		}
	    }

	#ifdef HAVE_PTHREADS
	    pthread_mutex_unlock(&log_init_lock);
	#endif

	    return write_to_log(log_id, vec, nr);
	}



	static int __write_to_log_kernel(log_id_t log_id, struct iovec *vec, size_t nr)
	{
	    ssize_t ret;
	    int log_fd;

	    if (/*(int)log_id >= 0 &&*/ (int)log_id < (int)LOG_ID_MAX) {
		log_fd = log_fds[(int)log_id];
	    } else {
		return EBADF;
	    }

	    do {
		ret = log_writev(log_fd, vec, nr);//	#define log_writev(filedes, vector, count) writev(filedes, vector, count)
	    } while (ret < 0 && errno == EINTR);

	    return ret;
	}


	//	system/core/liblog/uio.c
	int  writev( int  fd, const struct iovec*  vecs, int  count )
	{
	    int   total = 0;

	    for ( ; count > 0; count--, vecs++ ) {
		const char*  buf = (const char*)vecs->iov_base;
		int          len = (int)vecs->iov_len;
		
		while (len > 0) {
		    int  ret = write( fd, buf, len );
		    if (ret < 0) {
		        if (total == 0)
		            total = -1;
		        goto Exit;
		    }
		    if (ret == 0)
		        goto Exit;

		    total += ret;
		    buf   += ret;
		    len   -= ret;
		}
	    }
	Exit:    
	    return total;
	}









	

}


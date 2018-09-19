private static void dumpStackTraces(String tracesPath, ArrayList<Integer> firstPids,ProcessCpuTracker processCpuTracker, SparseArray<Boolean> lastPids, String[] nativeProcs) {
	// Use a FileObserver to detect when traces finish writing.
	// The order of traces is considered important to maintain for legibility.
	//创建文件监测实例
	FileObserver observer = new FileObserver(tracesPath, FileObserver.CLOSE_WRITE) {
	    @Override
	    public synchronized void onEvent(int event, String path) { notify(); }
	};

	try {
	    //启动文件监视
	    observer.startWatching();

	    // First collect all of the stacks of the most important pids.
	    //优先比较重要的进程
	    if (firstPids != null) {
		try {
		    int num = firstPids.size();
		    for (int i = 0; i < num; i++) {
		        synchronized (observer) {
			    //通过kill命令发送 Process.SIGNAL_QUIT==3来模拟，这里文件traces.txt权限为0666，所在文件夹/data/anr/权限为0755
		            Process.sendSignal(firstPids.get(i), Process.SIGNAL_QUIT);
			    //等待200毫秒超时，由FileObserver监测唤醒
		            observer.wait(200);  // Wait for write-close, give up after 200msec
		        }
		    }
		} catch (InterruptedException e) {
		    Log.wtf(TAG, e);
		}
	    }

	    //本地进程堆栈信息触发
	    // Next collect the stacks of the native pids
	    if (nativeProcs != null) {
		int[] pids = Process.getPidsForCommands(nativeProcs);//分析:native方法对应jni方法android_os_Process_getPidsForCommands	
		if (pids != null) {
		    for (int pid : pids) {
		        Debug.dumpNativeBacktraceToFile(pid, tracesPath);//dumpNativeBacktraceToFile 为native方法，对应android_os_Debug_dumpNativeBacktraceToFile
		    }
		}
	    }

	    // Lastly, measure CPU usage.
	    if (processCpuTracker != null) {
		processCpuTracker.init();
		System.gc();
		processCpuTracker.update();
		try {
		    synchronized (processCpuTracker) {
		        processCpuTracker.wait(500); // measure over 1/2 second.
		    }
		} catch (InterruptedException e) {
		}
		processCpuTracker.update();

		// We'll take the stack crawls of just the top apps using CPU.
		final int N = processCpuTracker.countWorkingStats();
		int numProcs = 0;
		for (int i=0; i<N && numProcs<5; i++) {
		    ProcessCpuTracker.Stats stats = processCpuTracker.getWorkingStats(i);
		    if (lastPids.indexOfKey(stats.pid) >= 0) {
		        numProcs++;
		        try {
		            synchronized (observer) {
		                Process.sendSignal(stats.pid, Process.SIGNAL_QUIT);
		                observer.wait(200);  // Wait for write-close, give up after 200msec
		            }
		        } catch (InterruptedException e) {
		            Log.wtf(TAG, e);
		        }

		    }
		}
	    }
	} finally {
	    observer.stopWatching();
	}
}


//通过/proc/%d/cmdline 查找进程pid
//	frameworks/base/core/jni/android_util_Process.cpp
jintArray android_os_Process_getPidsForCommands(JNIEnv* env, jobject clazz,jobjectArray commandNames)
{
    if (commandNames == NULL) {
        jniThrowNullPointerException(env, NULL);
        return NULL;
    }

    Vector<String8> commands;

    jsize count = env->GetArrayLength(commandNames);

    for (int i=0; i<count; i++) {
        jobject obj = env->GetObjectArrayElement(commandNames, i);
        if (obj != NULL) {
            const char* str8 = env->GetStringUTFChars((jstring)obj, NULL);
            if (str8 == NULL) {
                jniThrowNullPointerException(env, "Element in commandNames");
                return NULL;
            }
            commands.add(String8(str8));
            env->ReleaseStringUTFChars((jstring)obj, str8);
        } else {
            jniThrowNullPointerException(env, "Element in commandNames");
            return NULL;
        }
    }

    Vector<jint> pids;

    DIR *proc = opendir("/proc");
    if (proc == NULL) {
        fprintf(stderr, "/proc: %s\n", strerror(errno));
        return NULL;
    }

    struct dirent *d;
    while ((d = readdir(proc))) {
        int pid = atoi(d->d_name);
        if (pid <= 0) continue;

        char path[PATH_MAX];
        char data[PATH_MAX];
        snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            continue;
        }
        const int len = read(fd, data, sizeof(data)-1);
        close(fd);

        if (len < 0) {
            continue;
        }
        data[len] = 0;

        for (int i=0; i<len; i++) {
            if (data[i] == ' ') {
                data[i] = 0;
                break;
            }
        }

        for (size_t i=0; i<commands.size(); i++) {
            if (commands[i] == data) {
                pids.add(pid);
                break;
            }
        }
    }

    closedir(proc);

    jintArray pidArray = env->NewIntArray(pids.size());
    if (pidArray == NULL) {
        jniThrowException(env, "java/lang/OutOfMemoryError", NULL);
        return NULL;
    }

    if (pids.size() > 0) {
        env->SetIntArrayRegion(pidArray, 0, pids.size(), pids.array());
    }

    return pidArray;
}


//	frameworks/base/core/jni/android_os_Debug.cpp
static void android_os_Debug_dumpNativeBacktraceToFile(JNIEnv* env, jobject clazz,jint pid, jstring fileName)
{
    if (fileName == NULL) {
        jniThrowNullPointerException(env, "file == null");
        return;
    }
    const jchar* str = env->GetStringCritical(fileName, 0);
    String8 fileName8;
    if (str) {
        fileName8 = String8(str, env->GetStringLength(fileName));
        env->ReleaseStringCritical(fileName, str);
    }

    int fd = open(fileName8.string(), O_CREAT | O_WRONLY | O_NOFOLLOW, 0666);  /* -rw-rw-rw- */
    if (fd < 0) {
        fprintf(stderr, "Can't open %s: %s\n", fileName8.string(), strerror(errno));
        return;
    }

    if (lseek(fd, 0, SEEK_END) < 0) {
        fprintf(stderr, "lseek: %s\n", strerror(errno));
    } else {
        dump_backtrace_to_file(pid, fd); //分析 
    }

    close(fd);
}


//	system/core/libcutils/debugger.c
int dump_backtrace_to_file(pid_t tid, int fd) {
    //	system/core/include/cutils/debugger.h:26:#define DEBUGGER_SOCKET_NAME "android:debuggerd"
    int s = socket_local_client(DEBUGGER_SOCKET_NAME,ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    if (s < 0) {
        return -1;
    }

    debugger_msg_t msg;
    msg.tid = tid;
    msg.action = DEBUGGER_ACTION_DUMP_BACKTRACE;

    int result = 0;
    if (TEMP_FAILURE_RETRY(write(s, &msg, sizeof(msg))) != sizeof(msg)) {
        result = -1;
    } else {
        char ack;
        if (TEMP_FAILURE_RETRY(read(s, &ack, 1)) != 1) {
            result = -1;
        } else {
            char buffer[4096];
            ssize_t n;
            while ((n = TEMP_FAILURE_RETRY(read(s, buffer, sizeof(buffer)))) > 0) {
                if (TEMP_FAILURE_RETRY(write(fd, buffer, n)) != n) {
                    result = -1;
                    break;
                }
            }
        }
    }
    TEMP_FAILURE_RETRY(close(s));
    return result;
}









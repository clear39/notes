private void scanDirTracedLI(File dir, final int parseFlags, int scanFlags, long currentTime) {
	Trace.traceBegin(TRACE_TAG_PACKAGE_MANAGER, "scanDir [" + dir.getAbsolutePath() + "]");
	try {
		scanDirLI(dir, parseFlags, scanFlags, currentTime);
	} finally {
		Trace.traceEnd(TRACE_TAG_PACKAGE_MANAGER);
	}
}

public static boolean PackageInstallerService.isStageName(String name) {
	final boolean isFile = name.startsWith("vmdl") && name.endsWith(".tmp");
	final boolean isContainer = name.startsWith("smdl") && name.endsWith(".tmp");
	final boolean isLegacyContainer = name.startsWith("smdl2tmp");
	return isFile || isContainer || isLegacyContainer;
}

private void scanDirLI(File dir, int parseFlags, int scanFlags, long currentTime) {
	final File[] files = dir.listFiles();
	if (ArrayUtils.isEmpty(files)) {
		Log.d(TAG, "No files in app dir " + dir);
		return;
	}

	if (DEBUG_PACKAGE_SCANNING) {
		Log.d(TAG, "Scanning app dir " + dir + " scanFlags=" + scanFlags + " flags=0x" + Integer.toHexString(parseFlags));
	}
	ParallelPackageParser parallelPackageParser = new ParallelPackageParser(
			mSeparateProcesses, mOnlyCore, mMetrics, mCacheDir,mParallelPackageParserCallback);

	// Submit files for parsing in parallel
	int fileCount = 0;
	for (File file : files) {
		// isApkFile 判断文件后缀是否为.apk 
		// isStageName排除以"vmdl"开头，以".tmp"结尾 或者 以"smdl"开头，以".tmp"结尾或者是以"smdl2tmp"开头文件
		final boolean isPackage = (isApkFile(file) || file.isDirectory()) && !PackageInstallerService.isStageName(file.getName());
		if (!isPackage) {
			// Ignore entries which are not packages
			continue;
		}
		parallelPackageParser.submit(file, parseFlags);
		fileCount++;
	}

	// Process results one by one
	for (; fileCount > 0; fileCount--) {
		ParallelPackageParser.ParseResult parseResult = parallelPackageParser.take();
		Throwable throwable = parseResult.throwable;
		int errorCode = PackageManager.INSTALL_SUCCEEDED;

		if (throwable == null) {
			// Static shared libraries have synthetic package names
			if (parseResult.pkg.applicationInfo.isStaticSharedLibrary()) {
				renameStaticSharedLibraryPackage(parseResult.pkg);
			}
			try {
				if (errorCode == PackageManager.INSTALL_SUCCEEDED) {
					scanPackageLI(parseResult.pkg, parseResult.scanFile, parseFlags, scanFlags,currentTime, null);
				}
			} catch (PackageManagerException e) {
				errorCode = e.error;
				Slog.w(TAG, "Failed to scan " + parseResult.scanFile + ": " + e.getMessage());
			}
		} else if (throwable instanceof PackageParser.PackageParserException) {
			PackageParser.PackageParserException e = (PackageParser.PackageParserException)
					throwable;
			errorCode = e.error;
			Slog.w(TAG, "Failed to parse " + parseResult.scanFile + ": " + e.getMessage());
		} else {
			throw new IllegalStateException("Unexpected exception occurred while parsing "
					+ parseResult.scanFile, throwable);
		}

		// Delete invalid userdata apps
		if ((parseFlags & PackageParser.PARSE_IS_SYSTEM) == 0 && errorCode == PackageManager.INSTALL_FAILED_INVALID_APK) {
			logCriticalInfo(Log.WARN,"Deleting invalid package at " + parseResult.scanFile);
			removeCodePathLI(parseResult.scanFile);
		}
	}
	parallelPackageParser.close();
}

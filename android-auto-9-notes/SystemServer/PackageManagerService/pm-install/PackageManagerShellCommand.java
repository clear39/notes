

class PackageManagerShellCommand extends ShellCommand {

	private int runInstall() throws RemoteException {
        final PrintWriter pw = getOutPrintWriter();
        //解析参数
        final InstallParams params = makeInstallParams();
        final String inPath = getNextArg();

        setParamsSize(params, inPath);

        final int sessionId = doCreateSession(params.sessionParams,params.installerPackageName, params.userId);
        boolean abandonSession = true;
        try {
            if (inPath == null && params.sessionParams.sizeBytes == -1) {
                pw.println("Error: must either specify a package size or an APK file");
                return 1;
            }
            if (doWriteSplit(sessionId, inPath, params.sessionParams.sizeBytes, "base.apk", false /*logSuccess*/) != PackageInstaller.STATUS_SUCCESS) {
                return 1;
            }
            if (doCommitSession(sessionId, false /*logSuccess*/) != PackageInstaller.STATUS_SUCCESS) {
                return 1;
            }
            abandonSession = false;
            pw.println("Success");
            return 0;
        } finally {
            if (abandonSession) {
                try {
                    doAbandonSession(sessionId, false /*logSuccess*/);
                } catch (Exception ignore) {
                }
            }
        }
    }

    private static class InstallParams {
        SessionParams sessionParams;
        String installerPackageName;
        int userId = UserHandle.USER_ALL;
    }


    private void setParamsSize(InstallParams params, String inPath) {
        if (params.sessionParams.sizeBytes == -1 && !STDIN_PATH.equals(inPath)) {
            final ParcelFileDescriptor fd = openFileForSystem(inPath, "r");
            if (fd == null) {
                getErrPrintWriter().println("Error: Can't open file: " + inPath);
                throw new IllegalArgumentException("Error: Can't open file: " + inPath);
            }
            try {
                ApkLite baseApk = PackageParser.parseApkLite(fd.getFileDescriptor(), inPath, 0);
                PackageLite pkgLite = new PackageLite(null, baseApk, null, null, null, null, null, null);
                params.sessionParams.setSize(PackageHelper.calculateInstalledSize(
                        pkgLite, params.sessionParams.abiOverride, fd.getFileDescriptor()));
                        
            } catch (PackageParserException | IOException e) {
                getErrPrintWriter().println("Error: Failed to parse APK file: " + inPath);
                throw new IllegalArgumentException("Error: Failed to parse APK file: " + inPath, e);
            } finally {
                try {
                    fd.close();
                } catch (IOException e) {
                }
            }
        }
    }


    private int doCreateSession(SessionParams params, String installerPackageName, int userId) throws RemoteException {
        userId = translateUserId(userId, true /*allowAll*/, "runInstallCreate");
        if (userId == UserHandle.USER_ALL) {
            userId = UserHandle.USER_SYSTEM;
            params.installFlags |= PackageManager.INSTALL_ALL_USERS;
        }

        // final IPackageManager mInterface;
        final int sessionId = mInterface.getPackageInstaller().createSession(params, installerPackageName, userId);
        return sessionId;
    }

    private int doWriteSplit(int sessionId, String inPath, long sizeBytes, String splitName, boolean logSuccess) throws RemoteException {
        final PrintWriter pw = getOutPrintWriter();
        final ParcelFileDescriptor fd;
        if (STDIN_PATH.equals(inPath)) {
            fd = new ParcelFileDescriptor(getInFileDescriptor());
        } else if (inPath != null) {
            fd = openFileForSystem(inPath, "r");
            if (fd == null) {
                return -1;
            }
            sizeBytes = fd.getStatSize();
            if (sizeBytes < 0) {
                getErrPrintWriter().println("Unable to get size of: " + inPath);
                return -1;
            }
        } else {
            fd = new ParcelFileDescriptor(getInFileDescriptor());
        }
        if (sizeBytes <= 0) {
            getErrPrintWriter().println("Error: must specify a APK size");
            return 1;
        }

        PackageInstaller.Session session = null;
        try {
            session = new PackageInstaller.Session(mInterface.getPackageInstaller().openSession(sessionId));
            session.write(splitName, 0, sizeBytes, fd);

            if (logSuccess) {
                pw.println("Success: streamed " + sizeBytes + " bytes");
            }
            return 0;
        } catch (IOException e) {
            getErrPrintWriter().println("Error: failed to write; " + e.getMessage());
            return 1;
        } finally {
            IoUtils.closeQuietly(session);
        }
    }

    private int doCommitSession(int sessionId, boolean logSuccess) throws RemoteException {
        final PrintWriter pw = getOutPrintWriter();
        PackageInstaller.Session session = null;
        try {
            session = new PackageInstaller.Session(mInterface.getPackageInstaller().openSession(sessionId));

            // Sanity check that all .dm files match an apk.
            // (The installer does not support standalone .dm files and will not process them.)
            try {
                DexMetadataHelper.validateDexPaths(session.getNames());
            } catch (IllegalStateException | IOException e) {
                pw.println("Warning [Could not validate the dex paths: " + e.getMessage() + "]");
            }

            final LocalIntentReceiver receiver = new LocalIntentReceiver();
            session.commit(receiver.getIntentSender());

            final Intent result = receiver.getResult();
            final int status = result.getIntExtra(PackageInstaller.EXTRA_STATUS,PackageInstaller.STATUS_FAILURE);
            if (status == PackageInstaller.STATUS_SUCCESS) {
                if (logSuccess) {
                    pw.println("Success");
                }
            } else {
                pw.println("Failure [" + result.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE) + "]");
            }
            return status;
        } finally {
            IoUtils.closeQuietly(session);
        }
    }



}
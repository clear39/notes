//程序路径：frameworks/base/tools/aapt



/*
 * Parse args.
 */
int main(int argc, char* const argv[])
{
    char *prog = argv[0];
    Bundle bundle;
    bool wantUsage = false;
    int result = 1;    // pessimistically assume an error.
    int tolerance = 0;

    /* default to compression */
    bundle.setCompressionMethod(ZipEntry::kCompressDeflated);

    if (argc < 2) {
        wantUsage = true;
        goto bail;
    }

    if (argv[1][0] == 'v')
        bundle.setCommand(kCommandVersion);
    else if (argv[1][0] == 'd')
        bundle.setCommand(kCommandDump);
    else if (argv[1][0] == 'l')
        bundle.setCommand(kCommandList);
    else if (argv[1][0] == 'a')
        bundle.setCommand(kCommandAdd);
    else if (argv[1][0] == 'r')
        bundle.setCommand(kCommandRemove);
    else if (argv[1][0] == 'p')
        bundle.setCommand(kCommandPackage);
    else if (argv[1][0] == 'c')
        bundle.setCommand(kCommandCrunch);
    else if (argv[1][0] == 's')
        bundle.setCommand(kCommandSingleCrunch);
    else if (argv[1][0] == 'm')
        bundle.setCommand(kCommandDaemon);
    else {
        fprintf(stderr, "ERROR: Unknown command '%s'\n", argv[1]);
        wantUsage = true;
        goto bail;
    }
    argc -= 2;
    argv += 2;

    /*
     * Pull out flags.  We support "-fv" and "-f -v".
     */
    while (argc && argv[0][0] == '-') {
        /* flag(s) found */
        const char* cp = argv[0] +1;

        while (*cp != '\0') {
            switch (*cp) {
            case 'v':
                bundle.setVerbose(true);
                break;
            case 'a':
                bundle.setAndroidList(true);
                break;
            case 'c':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-c' option\n");
                    wantUsage = true;
                    goto bail;
                }
                bundle.addConfigurations(argv[0]);
                break;
            case 'f':
                bundle.setForce(true);
                break;
            case 'g':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-g' option\n");
                    wantUsage = true;
                    goto bail;
                }
                tolerance = atoi(argv[0]);
                bundle.setGrayscaleTolerance(tolerance);
                printf("%s: Images with deviation <= %d will be forced to grayscale.\n", prog, tolerance);
                break;
            case 'k':
                bundle.setJunkPath(true);
                break;
            case 'm':
                bundle.setMakePackageDirs(true);
                break;
#if 0
            case 'p':
                bundle.setPseudolocalize(true);
                break;
#endif
            case 'u':
                bundle.setUpdate(true);
                break;
            case 'x':
                bundle.setExtending(true);
                break;
            case 'z':
                bundle.setRequireLocalization(true);
                break;
            case 'j':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-j' option\n");
                    wantUsage = true;
                    goto bail;
                }
                convertPath(argv[0]);
                bundle.addJarFile(argv[0]);
                break;
            case 'A':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-A' option\n");
                    wantUsage = true;
                    goto bail;
                }
                convertPath(argv[0]);
                bundle.addAssetSourceDir(argv[0]);
                break;
            case 'G':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-G' option\n");
                    wantUsage = true;
                    goto bail;
                }
                convertPath(argv[0]);
                bundle.setProguardFile(argv[0]);
                break;
            case 'D':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-D' option\n");
                    wantUsage = true;
                    goto bail;
                }
                convertPath(argv[0]);
                bundle.setMainDexProguardFile(argv[0]);
                break;
            case 'I':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-I' option\n");
                    wantUsage = true;
                    goto bail;
                }
                convertPath(argv[0]);
                bundle.addPackageInclude(argv[0]);
                break;
            case 'F':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-F' option\n");
                    wantUsage = true;
                    goto bail;
                }
                convertPath(argv[0]);
                bundle.setOutputAPKFile(argv[0]);
                break;
            case 'J':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-J' option\n");
                    wantUsage = true;
                    goto bail;
                }
                convertPath(argv[0]);
                bundle.setRClassDir(argv[0]);
                break;
            case 'M':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-M' option\n");
                    wantUsage = true;
                    goto bail;
                }
                convertPath(argv[0]);
                bundle.setAndroidManifestFile(argv[0]);
                break;
            case 'P':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-P' option\n");
                    wantUsage = true;
                    goto bail;
                }
                convertPath(argv[0]);
                bundle.setPublicOutputFile(argv[0]);
                break;
            case 'S':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-S' option\n");
                    wantUsage = true;
                    goto bail;
                }
                convertPath(argv[0]);
                bundle.addResourceSourceDir(argv[0]);
                break;
            case 'C':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-C' option\n");
                    wantUsage = true;
                    goto bail;
                }
                convertPath(argv[0]);
                bundle.setCrunchedOutputDir(argv[0]);
                break;
            case 'i':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-i' option\n");
                    wantUsage = true;
                    goto bail;
                }
                convertPath(argv[0]);
                bundle.setSingleCrunchInputFile(argv[0]);
                break;
            case 'o':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-o' option\n");
                    wantUsage = true;
                    goto bail;
                }
                convertPath(argv[0]);
                bundle.setSingleCrunchOutputFile(argv[0]);
                break;
            case '0':
                argc--;
                argv++;
                if (!argc) {
                    fprintf(stderr, "ERROR: No argument supplied for '-e' option\n");
                    wantUsage = true;
                    goto bail;
                }
                if (argv[0][0] != 0) {
                    bundle.addNoCompressExtension(argv[0]);
                } else {
                    bundle.setCompressionMethod(ZipEntry::kCompressStored);
                }
                break;
            case '-':
                if (strcmp(cp, "-debug-mode") == 0) {
                    bundle.setDebugMode(true);
                } else if (strcmp(cp, "-min-sdk-version") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--min-sdk-version' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setMinSdkVersion(argv[0]);
                } else if (strcmp(cp, "-target-sdk-version") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--target-sdk-version' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setTargetSdkVersion(argv[0]);
                } else if (strcmp(cp, "-max-sdk-version") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--max-sdk-version' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setMaxSdkVersion(argv[0]);
                } else if (strcmp(cp, "-max-res-version") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--max-res-version' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setMaxResVersion(argv[0]);
                } else if (strcmp(cp, "-version-code") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--version-code' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setVersionCode(argv[0]);
                } else if (strcmp(cp, "-version-name") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--version-name' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setVersionName(argv[0]);
                } else if (strcmp(cp, "-replace-version") == 0) {
                    bundle.setReplaceVersion(true);
                } else if (strcmp(cp, "-values") == 0) {
                    bundle.setValues(true);
                } else if (strcmp(cp, "-include-meta-data") == 0) {
                    bundle.setIncludeMetaData(true);
                } else if (strcmp(cp, "-custom-package") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--custom-package' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setCustomPackage(argv[0]);
                } else if (strcmp(cp, "-extra-packages") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--extra-packages' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setExtraPackages(argv[0]);
                } else if (strcmp(cp, "-generate-dependencies") == 0) {
                    bundle.setGenDependencies(true);
                } else if (strcmp(cp, "-utf16") == 0) {
                    bundle.setWantUTF16(true);
                } else if (strcmp(cp, "-preferred-density") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--preferred-density' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setPreferredDensity(argv[0]);
                } else if (strcmp(cp, "-split") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--split' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.addSplitConfigurations(argv[0]);
                } else if (strcmp(cp, "-feature-of") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--feature-of' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setFeatureOfPackage(argv[0]);
                } else if (strcmp(cp, "-feature-after") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--feature-after' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setFeatureAfterPackage(argv[0]);
                } else if (strcmp(cp, "-rename-manifest-package") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--rename-manifest-package' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setManifestPackageNameOverride(argv[0]);
                } else if (strcmp(cp, "-rename-instrumentation-target-package") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--rename-instrumentation-target-package' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setInstrumentationPackageNameOverride(argv[0]);
                } else if (strcmp(cp, "-auto-add-overlay") == 0) {
                    bundle.setAutoAddOverlay(true);
                } else if (strcmp(cp, "-error-on-failed-insert") == 0) {
                    bundle.setErrorOnFailedInsert(true);
                } else if (strcmp(cp, "-error-on-missing-config-entry") == 0) {
                    bundle.setErrorOnMissingConfigEntry(true);
                } else if (strcmp(cp, "-output-text-symbols") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '-output-text-symbols' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setOutputTextSymbols(argv[0]);
                } else if (strcmp(cp, "-product") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--product' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setProduct(argv[0]);
                } else if (strcmp(cp, "-non-constant-id") == 0) {
                    bundle.setNonConstantId(true);
                } else if (strcmp(cp, "-skip-symbols-without-default-localization") == 0) {
                    bundle.setSkipSymbolsWithoutDefaultLocalization(true);
                } else if (strcmp(cp, "-shared-lib") == 0) {
                    bundle.setNonConstantId(true);
                    bundle.setBuildSharedLibrary(true);
                } else if (strcmp(cp, "-app-as-shared-lib") == 0) {
                    bundle.setNonConstantId(true);
                    bundle.setBuildAppAsSharedLibrary(true);
                } else if (strcmp(cp, "-no-crunch") == 0) {
                    bundle.setUseCrunchCache(true);
                } else if (strcmp(cp, "-ignore-assets") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for '--ignore-assets' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    gUserIgnoreAssets = argv[0];
                } else if (strcmp(cp, "-pseudo-localize") == 0) {
                    bundle.setPseudolocalize(PSEUDO_ACCENTED | PSEUDO_BIDI);
                } else if (strcmp(cp, "-no-version-vectors") == 0) {
                    bundle.setNoVersionVectors(true);
                } else if (strcmp(cp, "-no-version-transitions") == 0) {
                    bundle.setNoVersionTransitions(true);
                } else if (strcmp(cp, "-private-symbols") == 0) {
                    argc--;
                    argv++;
                    if (!argc) {
                        fprintf(stderr, "ERROR: No argument supplied for " "'--private-symbols' option\n");
                        wantUsage = true;
                        goto bail;
                    }
                    bundle.setPrivateSymbolsPackage(String8(argv[0]));
                } else {
                    fprintf(stderr, "ERROR: Unknown option '-%s'\n", cp);
                    wantUsage = true;
                    goto bail;
                }
                cp += strlen(cp) - 1;
                break;
            default:
                fprintf(stderr, "ERROR: Unknown flag '-%c'\n", *cp);
                wantUsage = true;
                goto bail;
            }

            cp++;
        }
        argc--;
        argv++;
    }

    /*
     * We're past the flags.  The rest all goes straight in.
     */
    bundle.setFileSpec(argv, argc);

    result = handleCommand(&bundle);

bail:
    if (wantUsage) {
        usage();
        result = 2;
    }

    //printf("--> returning %d\n", result);
    return result;
}

/*
 * Dispatch the command.
 */
int handleCommand(Bundle* bundle)
{
    printf("--- command %d (verbose=%d force=%d):\n",bundle->getCommand(), bundle->getVerbose(), bundle->getForce());

    for (int i = 0; i < bundle->getFileSpecCount(); i++)
        printf("  %d: '%s'\n", i, bundle->getFileSpecEntry(i));

    switch (bundle->getCommand()) {
    case kCommandVersion:      return doVersion(bundle);
    case kCommandList:         return doList(bundle);
    case kCommandDump:         return doDump(bundle);
    case kCommandAdd:          return doAdd(bundle);
    case kCommandRemove:       return doRemove(bundle);
    case kCommandPackage:      return doPackage(bundle);
    case kCommandCrunch:       return doCrunch(bundle);
    case kCommandSingleCrunch: return doSingleCrunch(bundle);
    case kCommandDaemon:       return runInDaemonMode(bundle);
    default:
        fprintf(stderr, "%s: requested command not yet supported\n", gProgName);
        return 1;
    }
}


/*
 * Show version info.  All the cool kids do it.
 */
int doVersion(Bundle* bundle)
{
    if (bundle->getFileSpecCount() != 0) {
        printf("(ignoring extra arguments)\n");
    }
    printf("Android Asset Packaging Tool, v0.2-" AAPT_VERSION "\n");

    return 0;
}



/*
 * Handle the "list" command, which can be a simple file dump or a verbose listing.
 *
 * The verbose listing closely matches the output of the Info-ZIP "unzip" command.
 */
int doList(Bundle* bundle)
{
    int result = 1;
    ZipFile* zip = NULL;
    const ZipEntry* entry;
    long totalUncLen, totalCompLen;
    const char* zipFileName;

    if (bundle->getFileSpecCount() != 1) {
        fprintf(stderr, "ERROR: specify zip file name (only)\n");
        goto bail;
    }
    zipFileName = bundle->getFileSpecEntry(0);

    zip = openReadOnly(zipFileName);
    if (zip == NULL) {
        goto bail;
    }

    int count, i;

    if (bundle->getVerbose()) {
        printf("Archive:  %s\n", zipFileName);
        printf(" Length   Method    Size  Ratio   Offset      Date  Time  CRC-32    Name\n");
        printf("--------  ------  ------- -----  -------      ----  ----  ------    ----\n");
    }

    totalUncLen = totalCompLen = 0;

    count = zip->getNumEntries();
    for (i = 0; i < count; i++) {
        entry = zip->getEntryByIndex(i);
        if (bundle->getVerbose()) {
            char dateBuf[32];
            time_t when;

            when = entry->getModWhen();
            strftime(dateBuf, sizeof(dateBuf), "%m-%d-%y %H:%M",localtime(&when));

            printf("%8ld  %-7.7s %7ld %3d%%  %8zd  %s  %08lx  %s\n",
                (long) entry->getUncompressedLen(),
                compressionName(entry->getCompressionMethod()),
                (long) entry->getCompressedLen(),
                calcPercent(entry->getUncompressedLen(),entry->getCompressedLen()),
                (size_t) entry->getLFHOffset(),
                dateBuf,
                entry->getCRC32(),
                entry->getFileName());
        } else {
            printf("%s\n", entry->getFileName());
        }

        totalUncLen += entry->getUncompressedLen();
        totalCompLen += entry->getCompressedLen();
    }

    if (bundle->getVerbose()) {
        printf(
        "--------          -------  ---                            -------\n");
        printf("%8ld          %7ld  %2d%%                            %d files\n",
            totalUncLen,
            totalCompLen,
            calcPercent(totalUncLen, totalCompLen),
            zip->getNumEntries());
    }

    if (bundle->getAndroidList()) {
        AssetManager assets;
        if (!assets.addAssetPath(String8(zipFileName), NULL)) {
            fprintf(stderr, "ERROR: list -a failed because assets could not be loaded\n");
            goto bail;
        }

#ifdef __ANDROID__
        static const bool kHaveAndroidOs = true;
#else
        static const bool kHaveAndroidOs = false;
#endif
        const ResTable& res = assets.getResources(false);
        if (!kHaveAndroidOs) {
            printf("\nResource table:\n");
            res.print(false);
        }

        Asset* manifestAsset = assets.openNonAsset("AndroidManifest.xml",Asset::ACCESS_BUFFER);
        if (manifestAsset == NULL) {
            printf("\nNo AndroidManifest.xml found.\n");
        } else {
            printf("\nAndroid manifest:\n");
            ResXMLTree tree;
            tree.setTo(manifestAsset->getBuffer(true),manifestAsset->getLength());
            printXMLBlock(&tree);
        }
        delete manifestAsset;
    }

    result = 0;

bail:
    delete zip;
    return result;
}






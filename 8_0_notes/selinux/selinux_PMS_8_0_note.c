public PackageManagerService(Context context, Installer installer,boolean factoryTest, boolean onlyCore) {
	。。。。。。

	synchronized (mInstallLock) {
	// writer
	synchronized (mPackages) {
		。。。。。。

		mFoundPolicyFile = SELinuxMMAC.readInstallPolicy();

		。。。。。。
	} // synchronized (mPackages)
	} // synchronized (mInstallLock)
	。。。。。。
}



/** Path to MAC permissions on system image */
private static final File[] MAC_PERMISSIONS =
{ new File(Environment.getRootDirectory(), "/etc/selinux/plat_mac_permissions.xml"),
  new File(Environment.getVendorDirectory(), "/etc/selinux/nonplat_mac_permissions.xml") };



/**
 * Load the mac_permissions.xml file containing all seinfo assignments used to
 * label apps. The loaded mac_permissions.xml file is determined by the
 * MAC_PERMISSIONS class variable which is set at class load time which itself
 * is based on the USE_OVERRIDE_POLICY class variable. For further guidance on
 * the proper structure of a mac_permissions.xml file consult the source code
 * located at system/sepolicy/mac_permissions.xml.
 *
 * @return boolean indicating if policy was correctly loaded. A value of false
 *         typically indicates a structural problem with the xml or incorrectly
 *         constructed policy stanzas. A value of true means that all stanzas
 *         were loaded successfully; no partial loading is possible.
 */
public static boolean readInstallPolicy() {
	// Temp structure to hold the rules while we parse the xml file
	List<Policy> policies = new ArrayList<>();

	FileReader policyFile = null;
	XmlPullParser parser = Xml.newPullParser();
	for (int i = 0; i < MAC_PERMISSIONS.length; i++) {
		try {
			policyFile = new FileReader(MAC_PERMISSIONS[i]);
			Slog.d(TAG, "Using policy file " + MAC_PERMISSIONS[i]);

			parser.setInput(policyFile);
			parser.nextTag();
			parser.require(XmlPullParser.START_TAG, null, "policy");

			while (parser.next() != XmlPullParser.END_TAG) {
				if (parser.getEventType() != XmlPullParser.START_TAG) {
					continue;
				}

				switch (parser.getName()) {
					case "signer":
						policies.add(readSignerOrThrow(parser));
						break;
					default:
						skip(parser);
				}
			}
		} catch (IllegalStateException | IllegalArgumentException | XmlPullParserException ex) {
			StringBuilder sb = new StringBuilder("Exception @");
			sb.append(parser.getPositionDescription());
			sb.append(" while parsing ");
			sb.append(MAC_PERMISSIONS[i]);
			sb.append(":");
			sb.append(ex);
			Slog.w(TAG, sb.toString());
			return false;
		} catch (IOException ioe) {
			Slog.w(TAG, "Exception parsing " + MAC_PERMISSIONS[i], ioe);
			return false;
		} finally {
			IoUtils.closeQuietly(policyFile);
		}
	}

	// Now sort the policy stanzas
	PolicyComparator policySort = new PolicyComparator();
	Collections.sort(policies, policySort);
	if (policySort.foundDuplicate()) {
		Slog.w(TAG, "ERROR! Duplicate entries found parsing mac_permissions.xml files");
		return false;
	}

	synchronized (sPolicies) {
		sPolicies = policies;
		if (DEBUG_POLICY_ORDER) {
			for (Policy policy : sPolicies) {
				Slog.d(TAG, "Policy: " + policy.toString());
			}
		}
	}

	return true;
}


/**
 * Loop over a signer tag looking for seinfo, package and cert tags. A {@link Policy}
 * instance will be created and returned in the process. During the pass all other
 * tag elements will be skipped.
 *
 * @param parser an XmlPullParser object representing a signer element.
 * @return the constructed {@link Policy} instance
 * @throws IOException
 * @throws XmlPullParserException
 * @throws IllegalArgumentException if any of the validation checks fail while
 *         parsing tag values.
 * @throws IllegalStateException if any of the invariants fail when constructing
 *         the {@link Policy} instance.
 */
private static Policy readSignerOrThrow(XmlPullParser parser) throws IOException,XmlPullParserException {

	parser.require(XmlPullParser.START_TAG, null, "signer");
	Policy.PolicyBuilder pb = new Policy.PolicyBuilder();

	// Check for a cert attached to the signer tag. We allow a signature
	// to appear as an attribute as well as those attached to cert tags.
	String cert = parser.getAttributeValue(null, "signature");
	if (cert != null) {
		pb.addSignature(cert);
	}

	while (parser.next() != XmlPullParser.END_TAG) {
		if (parser.getEventType() != XmlPullParser.START_TAG) {
			continue;
		}

		String tagName = parser.getName();
		if ("seinfo".equals(tagName)) {
			String seinfo = parser.getAttributeValue(null, "value");
			pb.setGlobalSeinfoOrThrow(seinfo);
			readSeinfo(parser);
		} else if ("package".equals(tagName)) {
			readPackageOrThrow(parser, pb);
		} else if ("cert".equals(tagName)) {
			String sig = parser.getAttributeValue(null, "signature");
			pb.addSignature(sig);
			readCert(parser);
		} else {
			skip(parser);
		}
	}

	return pb.build();
}




/**
 * Applies a security label to a package based on an seinfo tag taken from a matched
 * policy. All signature based policy stanzas are consulted and, if no match is
 * found, the default seinfo label of 'default' (set in ApplicationInfo object) is
 * used. The security label is attached to the ApplicationInfo instance of the package
 * in the event that a matching policy was found.
 *
 * @param pkg object representing the package to be labeled.
 */
public static void assignSeInfoValue(PackageParser.Package pkg) {
	synchronized (sPolicies) {
		for (Policy policy : sPolicies) {
			String seInfo = policy.getMatchedSeInfo(pkg);
			if (seInfo != null) {
				pkg.applicationInfo.seInfo = seInfo;
				break;
			}
		}
	}

	if (pkg.applicationInfo.targetSandboxVersion == 2)
		pkg.applicationInfo.seInfo += SANDBOX_V2_STR;

	if (pkg.applicationInfo.isPrivilegedApp())
		pkg.applicationInfo.seInfo += PRIVILEGED_APP_STR;

	pkg.applicationInfo.seInfo += TARGETSDKVERSION_STR + pkg.applicationInfo.targetSdkVersion;

	if (DEBUG_POLICY_INSTALL) {
		Slog.i(TAG, "package (" + pkg.packageName + ") labeled with " + "seinfo=" + pkg.applicationInfo.seInfo);
	}
}
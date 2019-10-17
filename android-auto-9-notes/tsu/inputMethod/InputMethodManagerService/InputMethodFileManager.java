//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/services/core/java/com/android/server/InputMethodManagerService.java

private static class InputMethodFileManager {

    public InputMethodFileManager(HashMap<String, InputMethodInfo> methodMap, int userId) {
        if (methodMap == null) {
            throw new NullPointerException("methodMap is null");
        }
        mMethodMap = methodMap;
        final File systemDir = userId == UserHandle.USER_SYSTEM
                ? new File(Environment.getDataDirectory(), SYSTEM_PATH)
                : Environment.getUserSystemDirectory(userId);
        /**
         * /data/system/inputmethod
         * */
        final File inputMethodDir = new File(systemDir, INPUT_METHOD_PATH);
        if (!inputMethodDir.exists() && !inputMethodDir.mkdirs()) {
            Slog.w(TAG, "Couldn't create dir.: " + inputMethodDir.getAbsolutePath());
        }
        /**
         * /data/system/inputmethod/subtypes.xml
         * */
        final File subtypeFile = new File(inputMethodDir, ADDITIONAL_SUBTYPES_FILE_NAME);
        mAdditionalInputMethodSubtypeFile = new AtomicFile(subtypeFile, "input-subtypes");
        if (!subtypeFile.exists()) {
            // If "subtypes.xml" doesn't exist, create a blank file.
            writeAdditionalInputMethodSubtypes(mAdditionalSubtypesMap, mAdditionalInputMethodSubtypeFile, methodMap);
        } else {
            readAdditionalInputMethodSubtypes(mAdditionalSubtypesMap, mAdditionalInputMethodSubtypeFile);
        }
    }

    /***
     * 
     * 如果 /data/system/inputmethod/subtypes.xml 不存在则调用
     * 
     * 将 allSubtypes 
     * 
     */
    private static void writeAdditionalInputMethodSubtypes(HashMap<String, List<InputMethodSubtype>> allSubtypes, AtomicFile subtypesFile,
                HashMap<String, InputMethodInfo> methodMap) {
            // Safety net for the case that this function is called before methodMap is set.
            final boolean isSetMethodMap = methodMap != null && methodMap.size() > 0;
            FileOutputStream fos = null;
            try {
                fos = subtypesFile.startWrite();
                final XmlSerializer out = new FastXmlSerializer();
                out.setOutput(fos, StandardCharsets.UTF_8.name());
                out.startDocument(null, true);
                out.setFeature("http://xmlpull.org/v1/doc/features.html#indent-output", true);
                out.startTag(null, NODE_SUBTYPES);
                for (String imiId : allSubtypes.keySet()) {
                    if (isSetMethodMap && !methodMap.containsKey(imiId)) {
                        Slog.w(TAG, "IME uninstalled or not valid.: " + imiId);
                        continue;
                    }
                    out.startTag(null, NODE_IMI);
                    out.attribute(null, ATTR_ID, imiId);
                    final List<InputMethodSubtype> subtypesList = allSubtypes.get(imiId);
                    final int N = subtypesList.size();
                    for (int i = 0; i < N; ++i) {
                        final InputMethodSubtype subtype = subtypesList.get(i);
                        out.startTag(null, NODE_SUBTYPE);
                        if (subtype.hasSubtypeId()) {
                            out.attribute(null, ATTR_IME_SUBTYPE_ID,String.valueOf(subtype.getSubtypeId()));
                        }
                        out.attribute(null, ATTR_ICON, String.valueOf(subtype.getIconResId()));
                        out.attribute(null, ATTR_LABEL, String.valueOf(subtype.getNameResId()));
                        out.attribute(null, ATTR_IME_SUBTYPE_LOCALE, subtype.getLocale());
                        out.attribute(null, ATTR_IME_SUBTYPE_LANGUAGE_TAG,subtype.getLanguageTag());
                        out.attribute(null, ATTR_IME_SUBTYPE_MODE, subtype.getMode());
                        out.attribute(null, ATTR_IME_SUBTYPE_EXTRA_VALUE, subtype.getExtraValue());
                        out.attribute(null, ATTR_IS_AUXILIARY,String.valueOf(subtype.isAuxiliary() ? 1 : 0));
                        out.attribute(null, ATTR_IS_ASCII_CAPABLE,String.valueOf(subtype.isAsciiCapable() ? 1 : 0));
                        out.endTag(null, NODE_SUBTYPE);
                    }
                    out.endTag(null, NODE_IMI);
                }
                out.endTag(null, NODE_SUBTYPES);
                out.endDocument();
                subtypesFile.finishWrite(fos);
            } catch (java.io.IOException e) {
                Slog.w(TAG, "Error writing subtypes", e);
                if (fos != null) {
                    subtypesFile.failWrite(fos);
                }
            }
        }

        private static void readAdditionalInputMethodSubtypes(HashMap<String, List<InputMethodSubtype>> allSubtypes, AtomicFile subtypesFile) {
            if (allSubtypes == null || subtypesFile == null) return;
            allSubtypes.clear();
            try (final FileInputStream fis = subtypesFile.openRead()) {
                final XmlPullParser parser = Xml.newPullParser();
                parser.setInput(fis, StandardCharsets.UTF_8.name());
                int type = parser.getEventType();
                // Skip parsing until START_TAG
                while ((type = parser.next()) != XmlPullParser.START_TAG && type != XmlPullParser.END_DOCUMENT) {

                }
                String firstNodeName = parser.getName();
                if (!NODE_SUBTYPES.equals(firstNodeName)) {
                    throw new XmlPullParserException("Xml doesn't start with subtypes");
                }
                final int depth =parser.getDepth();
                String currentImiId = null;
                ArrayList<InputMethodSubtype> tempSubtypesArray = null;
                while (((type = parser.next()) != XmlPullParser.END_TAG
                        || parser.getDepth() > depth) && type != XmlPullParser.END_DOCUMENT) {
                    if (type != XmlPullParser.START_TAG)
                        continue;
                    final String nodeName = parser.getName();
                    if (NODE_IMI.equals(nodeName)) {
                        currentImiId = parser.getAttributeValue(null, ATTR_ID);
                        if (TextUtils.isEmpty(currentImiId)) {
                            Slog.w(TAG, "Invalid imi id found in subtypes.xml");
                            continue;
                        }
                        tempSubtypesArray = new ArrayList<>();
                        allSubtypes.put(currentImiId, tempSubtypesArray);
                    } else if (NODE_SUBTYPE.equals(nodeName)) {
                        if (TextUtils.isEmpty(currentImiId) || tempSubtypesArray == null) {
                            Slog.w(TAG, "IME uninstalled or not valid.: " + currentImiId);
                            continue;
                        }
                        final int icon = Integer.parseInt(
                                parser.getAttributeValue(null, ATTR_ICON));
                        final int label = Integer.parseInt(
                                parser.getAttributeValue(null, ATTR_LABEL));
                        final String imeSubtypeLocale =
                                parser.getAttributeValue(null, ATTR_IME_SUBTYPE_LOCALE);
                        final String languageTag =
                                parser.getAttributeValue(null, ATTR_IME_SUBTYPE_LANGUAGE_TAG);
                        final String imeSubtypeMode =
                                parser.getAttributeValue(null, ATTR_IME_SUBTYPE_MODE);
                        final String imeSubtypeExtraValue =
                                parser.getAttributeValue(null, ATTR_IME_SUBTYPE_EXTRA_VALUE);
                        final boolean isAuxiliary = "1".equals(String.valueOf(
                                parser.getAttributeValue(null, ATTR_IS_AUXILIARY)));
                        final boolean isAsciiCapable = "1".equals(String.valueOf(
                                parser.getAttributeValue(null, ATTR_IS_ASCII_CAPABLE)));
                        final InputMethodSubtypeBuilder builder = new InputMethodSubtypeBuilder()
                                .setSubtypeNameResId(label)
                                .setSubtypeIconResId(icon)
                                .setSubtypeLocale(imeSubtypeLocale)
                                .setLanguageTag(languageTag)
                                .setSubtypeMode(imeSubtypeMode)
                                .setSubtypeExtraValue(imeSubtypeExtraValue)
                                .setIsAuxiliary(isAuxiliary)
                                .setIsAsciiCapable(isAsciiCapable);
                        final String subtypeIdString = parser.getAttributeValue(null, ATTR_IME_SUBTYPE_ID);
                        if (subtypeIdString != null) {
                            builder.setSubtypeId(Integer.parseInt(subtypeIdString));
                        }
                        tempSubtypesArray.add(builder.build());
                    }
                }
            } catch (XmlPullParserException | IOException | NumberFormatException e) {
                Slog.w(TAG, "Error reading subtypes", e);
                return;
            }
        }
    }










    

}
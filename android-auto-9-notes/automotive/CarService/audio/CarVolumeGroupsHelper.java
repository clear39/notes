

//  @    /work/workcodes/aosp-p9.x-auto-ga/packages/services/Car/service/src/com/android/car/CarVolumeGroupsHelper.java


/**
    36 <volumeGroups xmlns:car="http://schemas.android.com/apk/res-auto">
    37     <group>
    38     ┊   <context car:context="music"/>
    39     ┊   <context car:context="call_ring"/>
    40     ┊   <context car:context="navigation"/>
    41     ┊   <context car:context="voice_command"/>
    42     ┊   <context car:context="call"/>
    43     </group>
    44     <group>
    45     ┊   <context car:context="notification"/>
    46     ┊   <context car:context="system_sound"/>
    47     ┊   <context car:context="alarm"/>
    48     </group>
    49 </volumeGroups>

        */

/* package */ class CarVolumeGroupsHelper {

    CarVolumeGroupsHelper(Context context, @XmlRes int xmlConfiguration) {
        mContext = context;
        mXmlConfiguration = xmlConfiguration; // packages/services/Car/service/res/xml/car_volume_groups.xml
    }

    CarVolumeGroup[] loadVolumeGroups() {
        List<CarVolumeGroup> carVolumeGroups = new ArrayList<>();
        try (XmlResourceParser parser = mContext.getResources().getXml(mXmlConfiguration)) {
            AttributeSet attrs = Xml.asAttributeSet(parser);
            int type;
            // Traverse to the first start tag
            while ((type=parser.next()) != XmlResourceParser.END_DOCUMENT&& type != XmlResourceParser.START_TAG) {
            }
            /***
             * private static final String TAG_VOLUME_GROUPS = "volumeGroups";
             */
            if (!TAG_VOLUME_GROUPS.equals(parser.getName())) {
                throw new RuntimeException("Meta-data does not start with volumeGroups tag");
            }
            int outerDepth = parser.getDepth();
            int id = 0;
            while ((type=parser.next()) != XmlResourceParser.END_DOCUMENT && (type != XmlResourceParser.END_TAG || parser.getDepth() > outerDepth)) {
                if (type == XmlResourceParser.END_TAG) {
                    continue;
                }
                /***
                 * private static final String TAG_GROUP = "group";
                 */
                if (TAG_GROUP.equals(parser.getName())) {
                    carVolumeGroups.add(parseVolumeGroup(id, attrs, parser));
                    id++;
                }
            }
        } catch (Exception e) {
            Log.e(CarLog.TAG_AUDIO, "Error parsing volume groups configuration", e);
        }
        return carVolumeGroups.toArray(new CarVolumeGroup[carVolumeGroups.size()]);
    }


    private CarVolumeGroup parseVolumeGroup(int id, AttributeSet attrs, XmlResourceParser parser)
            throws XmlPullParserException, IOException {
        int type;

        List<Integer> contexts = new ArrayList<>();
        int innerDepth = parser.getDepth();
        while ((type=parser.next()) != XmlResourceParser.END_DOCUMENT
                && (type != XmlResourceParser.END_TAG || parser.getDepth() > innerDepth)) {
            if (type == XmlResourceParser.END_TAG) {
                continue;
            }
            /***
             *  private static final String TAG_CONTEXT = "context";
             */
            if (TAG_CONTEXT.equals(parser.getName())) {
                TypedArray c = mContext.getResources().obtainAttributes(attrs, R.styleable.volumeGroups_context);
                contexts.add(c.getInt(R.styleable.volumeGroups_context_context, -1));
                c.recycle();
            }
        }

        return new CarVolumeGroup(mContext, id,contexts.stream().mapToInt(i -> i).filter(i -> i >= 0).toArray());
    }



}
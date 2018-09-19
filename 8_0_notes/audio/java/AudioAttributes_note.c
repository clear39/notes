/**
 * A class to encapsulate(封装) a collection of attributes describing information about an audio
 * stream.

 * <p><code>AudioAttributes</code> supersede(取代) the notion(概念、观念) of stream types (see for instance
 * {@link AudioManager#STREAM_MUSIC} or {@link AudioManager#STREAM_ALARM}) for defining the
 * behavior of audio playback. Attributes allow an application to specify more information than is
 * conveyed(传达) in a stream type by allowing the application to define:
 * <ul>
 * <li>usage: "why" you are playing a sound, what is this sound used for. This is achieved(实现,完美的) with
 *     the "usage" information. Examples of usage are {@link #USAGE_MEDIA} and {@link #USAGE_ALARM}.
 *     These two examples are the closest（最靠近的） to stream types, but more detailed use cases(案例) are
 *     available. Usage information is more expressive（有表现力的） than a stream type, and allows certain（某些）
 *     platforms or routing policies to use this information for more refined volume or routing
 *     decisions. Usage is the most important information to supply in <code>AudioAttributes</code>
 *     and it is recommended to build any instance with this information supplied, see
 *     {@link AudioAttributes.Builder} for exceptions.</li>
 * <li>content type: "what" you are playing. The content type expresses(快件) the general category of
 *     the content. This information is optional. But in case it is known (for instance
 *     {@link #CONTENT_TYPE_MOVIE} for a movie streaming service or {@link #CONTENT_TYPE_MUSIC} for
 *     a music playback application) this information might be used by the audio framework to
 *     selectively configure some audio post-processing blocks.</li>
 * <li>flags: "how" is playback to be affected(影响), see the flag definitions for the specific playback
 *     behaviors they control. </li>
 * </ul>
 * <p><code>AudioAttributes</code> are used for example in one of the {@link AudioTrack}
 * constructors (see {@link AudioTrack#AudioTrack(AudioAttributes, AudioFormat, int, int, int)}),
 * to configure a {@link MediaPlayer}
 * (see {@link MediaPlayer#setAudioAttributes(AudioAttributes)} or a
 * {@link android.app.Notification} (see {@link android.app.Notification#audioAttributes}). An
 * <code>AudioAttributes</code> instance is built through its builder,
 * {@link AudioAttributes.Builder}.
 */


public final class AudioAttributes implements Parcelable {
 /**
     * Content type value to use when the content type is unknown, or other than the ones defined.
     */
    public final static int CONTENT_TYPE_UNKNOWN = 0;
    /**
     * Content type value to use when the content type is speech.
     */
    public final static int CONTENT_TYPE_SPEECH = 1;
    /**
     * Content type value to use when the content type is music.
     */
    public final static int CONTENT_TYPE_MUSIC = 2;
    /**
     * Content type value to use when the content type is a soundtrack, typically accompanying
     * a movie or TV program.
     */
    public final static int CONTENT_TYPE_MOVIE = 3;
    /**
     * Content type value to use when the content type is a sound used to accompany(陪伴) a user
     * action, such as a beep or sound effect expressing(表达) a key click, or event, such as the
     * type of a sound for a bonus being received in a game. These sounds are mostly synthesized
     * or short Foley sounds.
     */
    public final static int CONTENT_TYPE_SONIFICATION = 4;




    /**
     * Usage value to use when the usage is unknown.
     */
    public final static int USAGE_UNKNOWN = 0;
    /**
     * Usage value to use when the usage is media, such as music, or movie
     * soundtracks.
     */
    public final static int USAGE_MEDIA = 1;
    /**
     * Usage value to use when the usage is voice communications, such as telephony
     * or VoIP(网络电话).
     */
    public final static int USAGE_VOICE_COMMUNICATION = 2;
    /**
     * Usage value to use when the usage is in-call signalling(信令), such as with
     * a "busy" beep, or DTMF tones.
     */
    public final static int USAGE_VOICE_COMMUNICATION_SIGNALLING = 3;
    /**
     * Usage value to use when the usage is an alarm (e.g. wake-up alarm).
     */
    public final static int USAGE_ALARM = 4;
    /**
     * Usage value to use when the usage is notification. See other
     * notification usages for more specialized uses.
     */
    public final static int USAGE_NOTIFICATION = 5;
    /**
     * Usage value to use when the usage is telephony ringtone.
     */
    public final static int USAGE_NOTIFICATION_RINGTONE = 6;
    /**
     * Usage value to use when the usage is a request to enter/end a
     * communication, such as a VoIP communication or video-conference.
     */
    public final static int USAGE_NOTIFICATION_COMMUNICATION_REQUEST = 7;
    /**
     * Usage value to use when the usage is notification for an "instant(瞬间)"
     * communication such as a chat, or SMS.
     */
    public final static int USAGE_NOTIFICATION_COMMUNICATION_INSTANT = 8;
    /**
     * Usage value to use when the usage is notification for a
     * non-immediate(非即时的) type of communication such as e-mail.
     */
    public final static int USAGE_NOTIFICATION_COMMUNICATION_DELAYED = 9;
    /**
     * Usage value to use when the usage is to attract the user's attention,
     * such as a reminder or low battery warning.
     */
    public final static int USAGE_NOTIFICATION_EVENT = 10;
    /**
     * Usage value to use when the usage is for accessibility, such as with
     * a screen reader.
     */
    public final static int USAGE_ASSISTANCE_ACCESSIBILITY = 11;
    /**
     * Usage value to use when the usage is driving or navigation directions.
     */
    public final static int USAGE_ASSISTANCE_NAVIGATION_GUIDANCE = 12;
    /**
     * Usage value to use when the usage is sonification, such as  with user
     * interface sounds.
     */
    public final static int USAGE_ASSISTANCE_SONIFICATION = 13;
    /**
     * Usage value to use when the usage is for game audio.
     */
    public final static int USAGE_GAME = 14;
    /**
     * @hide
     * Usage value to use when feeding audio to the platform and replacing "traditional" audio
     * source, such as audio capture devices.
     */
    public final static int USAGE_VIRTUAL_SOURCE = 15;
    /**
     * Usage value to use for audio responses to user queries, audio instructions or help
     * utterances.
     */
    public final static int USAGE_ASSISTANT = 16;




















}

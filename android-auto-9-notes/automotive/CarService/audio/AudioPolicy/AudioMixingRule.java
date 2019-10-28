
//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/media/java/android/media/audiopolicy/AudioMixingRule.java






public static class Builder {
    public Builder() {
        mCriteria = new ArrayList<AudioMixMatchCriterion>();
    }


    /**
     * Add a rule for the selection of which streams are mixed together.
     * @param attrToMatch a non-null AudioAttributes instance for which a contradictory
     *     rule hasn't been set yet.
     * @param rule {@link AudioMixingRule#RULE_MATCH_ATTRIBUTE_USAGE} or
     *     {@link AudioMixingRule#RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET}.
     * @return the same Builder instance.
     * @throws IllegalArgumentException
     * @see #excludeRule(AudioAttributes, int)
     */
    @SystemApi
    public Builder addRule(AudioAttributes attrToMatch, int rule)throws IllegalArgumentException {
        if (!isValidAttributesSystemApiRule(rule)) {
            throw new IllegalArgumentException("Illegal rule value " + rule);
        }
        return checkAddRuleObjInternal(rule, attrToMatch);
    }

    private static boolean isValidAttributesSystemApiRule(int rule) {
        // API rules only expose the RULE_MATCH_* rules
        switch (rule) {
            case RULE_MATCH_ATTRIBUTE_USAGE:
            case RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET:
                return true;
            default:
                return false;
        }
    }

    /**
     * Add or exclude a rule for the selection of which streams are mixed together.
     * Does error checking on the parameters.
     * @param rule
     * @param property
     * @return the same Builder instance.
     * @throws IllegalArgumentException
     */
    private Builder checkAddRuleObjInternal(int rule, Object property)throws IllegalArgumentException {
        if (property == null) {
            throw new IllegalArgumentException("Illegal null argument for mixing rule");
        }
        if (!isValidRule(rule)) {
            throw new IllegalArgumentException("Illegal rule value " + rule);
        }
        final int match_rule = rule & ~RULE_EXCLUSION_MASK;
        if (isAudioAttributeRule(match_rule)) {
            if (!(property instanceof AudioAttributes)) {
                throw new IllegalArgumentException("Invalid AudioAttributes argument");
            }
            return addRuleInternal((AudioAttributes) property, null, rule);
        } else {// uid
            // implies integer match rule
            if (!(property instanceof Integer)) {
                throw new IllegalArgumentException("Invalid Integer argument");
            }
            return addRuleInternal(null, (Integer) property, rule);
        }
    }

    private static boolean isValidRule(int rule) {
        final int match_rule = rule & ~RULE_EXCLUSION_MASK;
        switch (match_rule) {
            case RULE_MATCH_ATTRIBUTE_USAGE:
            case RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET:
            case RULE_MATCH_UID:
                return true;
            default:
                return false;
        }
    }

    private static boolean isAudioAttributeRule(int match_rule) {
        switch(match_rule) {
            case RULE_MATCH_ATTRIBUTE_USAGE:
            case RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET:
                return true;
            default:
                return false;
        }
    }


    private Builder addRuleInternal(AudioAttributes attrToMatch, Integer intProp, int rule) throws IllegalArgumentException {
        // as rules are added to the Builder, we verify they are consistent with the type
        // of mix being built. When adding the first rule, the mix type is MIX_TYPE_INVALID.
        if (mTargetMixType == AudioMix.MIX_TYPE_INVALID) {
            /*
            private static boolean isPlayerRule(int rule) {
                final int match_rule = rule & ~RULE_EXCLUSION_MASK;
                switch (match_rule) {
                case RULE_MATCH_ATTRIBUTE_USAGE:
                case RULE_MATCH_UID:
                    return true;
                default:
                    return false;
                }
            }
            */
            if (isPlayerRule(rule)) {
                mTargetMixType = AudioMix.MIX_TYPE_PLAYERS;
            } else {
                mTargetMixType = AudioMix.MIX_TYPE_RECORDERS;
            }
        } else if (((mTargetMixType == AudioMix.MIX_TYPE_PLAYERS) && !isPlayerRule(rule))
                || ((mTargetMixType == AudioMix.MIX_TYPE_RECORDERS) && isPlayerRule(rule)))
        {
            throw new IllegalArgumentException("Incompatible rule for mix");
        }
        synchronized (mCriteria) {
            Iterator<AudioMixMatchCriterion> crIterator = mCriteria.iterator();
            final int match_rule = rule & ~RULE_EXCLUSION_MASK;
            while (crIterator.hasNext()) {
                final AudioMixMatchCriterion criterion = crIterator.next();
                switch (match_rule) {
                    case RULE_MATCH_ATTRIBUTE_USAGE:
                        // "usage"-based rule
                        if (criterion.mAttr.getUsage() == attrToMatch.getUsage()) {
                            if (criterion.mRule == rule) {
                                // rule already exists, we're done
                                return this;
                            } else {
                                // criterion already exists with a another rule,
                                // it is incompatible
                                throw new IllegalArgumentException("Contradictory rule exists"  + " for " + attrToMatch);
                            }
                        }
                        break;
                    case RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET:
                        // "capture preset"-base rule
                        if (criterion.mAttr.getCapturePreset() == attrToMatch.getCapturePreset()) {
                            if (criterion.mRule == rule) {
                                // rule already exists, we're done
                                return this;
                            } else {
                                // criterion already exists with a another rule,
                                // it is incompatible
                                throw new IllegalArgumentException("Contradictory rule exists"
                                        + " for " + attrToMatch);
                            }
                        }
                        break;
                    case RULE_MATCH_UID:
                        // "usage"-based rule
                        if (criterion.mIntProp == intProp.intValue()) {
                            if (criterion.mRule == rule) {
                                // rule already exists, we're done
                                return this;
                            } else {
                                // criterion already exists with a another rule,
                                // it is incompatible
                                throw new IllegalArgumentException("Contradictory rule exists"
                                        + " for UID " + intProp);
                            }
                        }
                        break;
                }
            }
            // rule didn't exist, add it
            switch (match_rule) {
                case RULE_MATCH_ATTRIBUTE_USAGE:
                case RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET:
                    mCriteria.add(new AudioMixMatchCriterion(attrToMatch, rule));
                    break;
                case RULE_MATCH_UID:
                    mCriteria.add(new AudioMixMatchCriterion(intProp, rule));
                    break;
                default:
                    throw new IllegalStateException("Unreachable code in addRuleInternal()");
            }
        }
        return this;
    }


}
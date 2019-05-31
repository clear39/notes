//	@system/tools/hidl/utils/FQName.cpp
FQName::FQName(const std::string &s)
    : mValid(false),		//bool mValid;
      mIsIdentifier(false) {//bool mIsIdentifier;
    setTo(s);
}


bool FQName::setTo(const std::string &s) {
    clearVersion();	//	mMajor = mMinor = 0;
    mPackage.clear();
    mName.clear();

    mValid = true;

    std::smatch match;
    if (std::regex_match(s, match, kRE1)) {
        CHECK_EQ(match.size(), 5u);

        mPackage = match.str(1);
        parseVersion(match.str(2), match.str(3));
        mName = match.str(4);
    } else if (std::regex_match(s, match, kRE2)) {
        CHECK_EQ(match.size(), 4u);

        parseVersion(match.str(1), match.str(2));
        mName = match.str(3);
    } else if (std::regex_match(s, match, kRE3)) {
        CHECK_EQ(match.size(), 4u);

        mPackage = match.str(1);
        parseVersion(match.str(2), match.str(3));
    } else if (std::regex_match(s, match, kRE4)) {
        mName = match.str(0);
    } else if (std::regex_match(s, match, kRE5)) {
        mIsIdentifier = true;
        mName = match.str(0);
    } else if (std::regex_match(s, match, kRE6)) {
        CHECK_EQ(match.size(), 6u);

        mPackage = match.str(1);
        parseVersion(match.str(2), match.str(3));
        mName = match.str(4);
        mValueName = match.str(5);
    } else if (std::regex_match(s, match, kRE7)) {
        CHECK_EQ(match.size(), 5u);

        parseVersion(match.str(1), match.str(2));
        mName = match.str(3);
        mValueName = match.str(4);
    } else if (std::regex_match(s, match, kRE8)) {
        CHECK_EQ(match.size(), 3u);

        mName = match.str(1);
        mValueName = match.str(2);
    } else {
        mValid = false;
    }

    // mValueName must go with mName.
    CHECK(mValueName.empty() || !mName.empty());

    // package without version is not allowed.
    CHECK(mPackage.empty() || !version().empty());

    return isValid();
}



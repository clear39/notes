


ArgumentBuilder::ArgumentBuilder(CmdlineParser::Builder& parser,
                std::shared_ptr<SaveDestination> save_destination)
    : parent_(parser),
        save_value_specified_(false),
        load_value_specified_(false),
        save_destination_(save_destination) {
        save_value_ = [](TArg&) {
            assert(false && "No save value function defined");
        };

        load_value_ = []() -> TArg& {
            assert(false && "No load value function defined");
            __builtin_trap();  // Blow up.
        };
}



void ArgumentBuilder::SetValuesInternal(const std::vector<TArg>&& value_list) {
    assert(!argument_info_.has_value_map_);

    //  detail::CmdlineParserArgumentInfo<TArg> argument_info_;
    argument_info_.has_value_list_ = true;
    argument_info_.value_list_ = value_list;
}


void ArgumentBuilder::SetNames(std::vector<const char*>&& names) {
    argument_info_.names_ = names;
}
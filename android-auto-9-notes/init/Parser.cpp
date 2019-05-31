//  @/work/workcodes/aosp-p9.x-auto-beta/system/core/init/parser.cpp
Parser::Parser() {}


void Parser::AddSectionParser(const std::string& name, std::unique_ptr<SectionParser> parser) {
    //  std::map<std::string, std::unique_ptr<SectionParser>> section_parsers_;
    section_parsers_[name] = std::move(parser);
}



bool Parser::ParseConfig(const std::string& path) {
    size_t parse_errors;
    return ParseConfig(path, &parse_errors);
}

bool Parser::ParseConfig(const std::string& path, size_t* parse_errors) {
    *parse_errors = 0;
    if (is_dir(path.c_str())) {
        return ParseConfigDir(path, parse_errors);
    }
    return ParseConfigFile(path, parse_errors);
}

//注意这里目录文件查找只会对当前目录文件查找，不会递归查找
bool Parser::ParseConfigDir(const std::string& path, size_t* parse_errors) {
    LOG(INFO) << "Parsing directory " << path << "...";
    std::unique_ptr<DIR, decltype(&closedir)> config_dir(opendir(path.c_str()), closedir);
    if (!config_dir) {
        PLOG(ERROR) << "Could not import directory '" << path << "'";
        return false;
    }
    dirent* current_file;
    std::vector<std::string> files;
    while ((current_file = readdir(config_dir.get()))) {
        // Ignore directories and only process regular files.
        if (current_file->d_type == DT_REG) {
            std::string current_path = android::base::StringPrintf("%s/%s", path.c_str(), current_file->d_name);
            files.emplace_back(current_path);
        }
    }
    // Sort first so we load files in a consistent order (bug 31996208)
    std::sort(files.begin(), files.end());
    for (const auto& file : files) {
        if (!ParseConfigFile(file, parse_errors)) {
            LOG(ERROR) << "could not import file '" << file << "'";
        }
    }
    return true;
}


bool Parser::ParseConfigFile(const std::string& path, size_t* parse_errors) {
    LOG(INFO) << "Parsing file " << path << "...";
    android::base::Timer t;
    auto config_contents = ReadFile(path);
    if (!config_contents) {
        LOG(ERROR) << "Unable to read config file '" << path << "': " << config_contents.error();
        return false;
    }

    config_contents->push_back('\n');  // TODO: fix parse_config.
    ParseData(path, *config_contents, parse_errors);
    // 注意 section_parsers_ 再构造是把 Parser 传递进去了
    for (const auto& [section_name, section_parser] : section_parsers_) {
        section_parser->EndFile();
    }

    LOG(VERBOSE) << "(Parsing " << path << " took " << t << ".)";
    return true;
}



void Parser::ParseData(const std::string& filename, const std::string& data, size_t* parse_errors) {
    // TODO: Use a parser with const input and remove this copy
    std::vector<char> data_copy(data.begin(), data.end());
    data_copy.push_back('\0');

    parse_state state;
    state.line = 0;
    state.ptr = &data_copy[0];
    state.nexttoken = 0;

    SectionParser* section_parser = nullptr;
    int section_start_line = -1;
    std::vector<std::string> args;

    auto end_section = [&] {
        if (section_parser == nullptr) return;

        if (auto result = section_parser->EndSection(); !result) {
            (*parse_errors)++;
            LOG(ERROR) << filename << ": " << section_start_line << ": " << result.error();
        }

        section_parser = nullptr;
        section_start_line = -1;
    };

    for (;;) {
        switch (next_token(&state)) {
            case T_EOF:
                end_section();
                return;
            case T_NEWLINE:
                state.line++;
                if (args.empty()) break;
                // If we have a line matching a prefix we recognize, call its callback and unset any
                // current section parsers.  This is meant for /sys/ and /dev/ line entries for
                // uevent.
                for (const auto& [prefix, callback] : line_callbacks_) {
                    if (android::base::StartsWith(args[0], prefix)) {
                        end_section();

                        if (auto result = callback(std::move(args)); !result) {
                            (*parse_errors)++;
                            LOG(ERROR) << filename << ": " << state.line << ": " << result.error();
                        }
                        break;
                    }
                }
                if (section_parsers_.count(args[0])) {
                    end_section();
                    section_parser = section_parsers_[args[0]].get();
                    section_start_line = state.line;
                    if (auto result = section_parser->ParseSection(std::move(args), filename, state.line); !result) {
                        (*parse_errors)++;
                        LOG(ERROR) << filename << ": " << state.line << ": " << result.error();
                        section_parser = nullptr;
                    }
                } else if (section_parser) {
                    if (auto result = section_parser->ParseLineSection(std::move(args), state.line);
                        !result) {
                        (*parse_errors)++;
                        LOG(ERROR) << filename << ": " << state.line << ": " << result.error();
                    }
                }
                args.clear();
                break;
            case T_TEXT:
                args.emplace_back(state.text);
                break;
        }
    }
}
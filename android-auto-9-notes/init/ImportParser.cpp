class ImportParser : public SectionParser {}

//  @/work/workcodes/aosp-p9.x-auto-ga/system/core/init/import_parser.cpp
ImportParser::ImportParser(Parser* parser) : parser_(parser) {}


Result<Success> ImportParser::ParseSection(std::vector<std::string>&& args,const std::string& filename, int line) {
    if (args.size() != 2) {
        return Error() << "single argument needed for import\n";
    }

    std::string conf_file;
    bool ret = expand_props(args[1], &conf_file);
    if (!ret) {
        return Error() << "error while expanding import";
    }

    LOG(INFO) << "Added '" << conf_file << "' to import list";
    if (filename_.empty()) filename_ = filename;
    imports_.emplace_back(std::move(conf_file), line);
    return Success();
}

void ImportParser::EndFile() {
    auto current_imports = std::move(imports_);
    imports_.clear();
    for (const auto& [import, line_num] : current_imports) {
        if (!parser_->ParseConfig(import)) {
            PLOG(ERROR) << filename_ << ": " << line_num << ": Could not import file '" << import
                        << "'";
        }
    }
}

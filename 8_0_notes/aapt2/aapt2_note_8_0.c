工程路径:frameworks/base/tools/aapt2


//代码入口	frameworks/base/tools/aapt2/Main.cpp
int main(int argc, char** argv) {
  if (argc >= 2) {
    argv += 1;
    argc -= 1;

    //	frameworks/base/libs/androidfw/include/androidfw/StringPiece.h
    //	using StringPiece = BasicStringPiece<char>; 
    //  字符串保存类，成员有char* data; int length;
    std::vector<android::StringPiece> args;
    for (int i = 1; i < argc; i++) {
      args.push_back(argv[i]);
    }

    android::StringPiece command(argv[0]);
    if (command == "compile" || command == "c") {
      aapt::StdErrDiagnostics diagnostics;
      return aapt::Compile(args, &diagnostics);
    } else if (command == "link" || command == "l") {
      aapt::StdErrDiagnostics diagnostics;
      return aapt::Link(args, &diagnostics);
    } else if (command == "dump" || command == "d") {
      return aapt::Dump(args);
    } else if (command == "diff") {
      return aapt::Diff(args);
    } else if (command == "optimize") {
      return aapt::Optimize(args);
    } else if (command == "version") {
      return aapt::PrintVersion();
    }
    std::cerr << "unknown command '" << command << "'\n";

  } else {
    std::cerr << "no command specified\n";
  }

  std::cerr << "\nusage: aapt2 [compile|link|dump|diff|optimize|version] ..." << std::endl;
  return 1;
}


//	frameworks/base/tools/aapt2/Diagnostics.h:68:struct IDiagnostics {

// 	frameworks/base/tools/aapt2/Diagnostics.h:91:class StdErrDiagnostics : public IDiagnostics {


/**
*	解析
*	aapt::StdErrDiagnostics diagnostics;
*	return aapt::Compile(args, &diagnostics);
*/
//	frameworks/base/tools/aapt2/cmd/Compile.cpp:660:
/**
 * Entry point for compilation phase. Parses arguments and dispatches to the
 * correct steps.
 */
int Compile(const std::vector<StringPiece>& args, IDiagnostics* diagnostics) {
  CompileContext context(diagnostics);
  CompileOptions options;

  bool verbose = false;
  Flags flags = Flags().RequiredFlag("-o", "Output path", &options.output_path)
          .OptionalFlag("--dir", "Directory to scan for resources", &options.res_dir)
          .OptionalSwitch("--pseudo-localize","Generate resources for pseudo-locales " "(en-XA and ar-XB)",&options.pseudolocalize)
          .OptionalSwitch("--no-crunch", "Disables PNG processing", &options.no_png_crunch)
          .OptionalSwitch("--legacy", "Treat errors that used to be valid in AAPT as warnings",&options.legacy_mode)
          .OptionalSwitch("-v", "Enables verbose logging", &verbose);

  if (!flags.Parse("aapt2 compile", args, &std::cerr)) {
    return 1;
  }

  context.SetVerbose(verbose);

  std::unique_ptr<IArchiveWriter> archive_writer;

  std::vector<ResourcePathData> input_data;
  if (options.res_dir) {
    if (!flags.GetArgs().empty()) {
      // Can't have both files and a resource directory.
      context.GetDiagnostics()->Error(DiagMessage() << "files given but --dir specified");
      flags.Usage("aapt2 compile", &std::cerr);
      return 1;
    }

    if (!LoadInputFilesFromDir(&context, options, &input_data)) {
      return 1;
    }

    archive_writer = CreateZipFileArchiveWriter(context.GetDiagnostics(), options.output_path);

  } else {
    input_data.reserve(flags.GetArgs().size());

    // Collect data from the path for each input file.
    for (const std::string& arg : flags.GetArgs()) {
      std::string error_str;
      if (Maybe<ResourcePathData> path_data = ExtractResourcePathData(arg, &error_str)) {
        input_data.push_back(std::move(path_data.value()));
      } else {
        context.GetDiagnostics()->Error(DiagMessage() << error_str << " (" << arg << ")");
        return 1;
      }
    }

    archive_writer = CreateDirectoryArchiveWriter(context.GetDiagnostics(), options.output_path);
  }

  if (!archive_writer) {
    return 1;
  }

  bool error = false;
  for (ResourcePathData& path_data : input_data) {
    if (options.verbose) {
      context.GetDiagnostics()->Note(DiagMessage(path_data.source) << "processing");
    }

    if (!IsValidFile(&context, path_data.source.path)) {
      error = true;
      continue;
    }

    if (path_data.resource_dir == "values") {
      // Overwrite the extension.
      path_data.extension = "arsc";

      const std::string output_filename = BuildIntermediateFilename(path_data);
      if (!CompileTable(&context, options, path_data, archive_writer.get(), output_filename)) {
        error = true;
      }

    } else {
      const std::string output_filename = BuildIntermediateFilename(path_data);
      if (const ResourceType* type = ParseResourceType(path_data.resource_dir)) {
        if (*type != ResourceType::kRaw) {
          if (path_data.extension == "xml") {
            if (!CompileXml(&context, options, path_data, archive_writer.get(), output_filename)) {
              error = true;
            }
          } else if (!options.no_png_crunch && (path_data.extension == "png" || path_data.extension == "9.png")) {
            if (!CompilePng(&context, options, path_data, archive_writer.get(), output_filename)) {
              error = true;
            }
          } else {
            if (!CompileFile(&context, options, path_data, archive_writer.get(), output_filename)) {
              error = true;
            }
          }
        } else {
          if (!CompileFile(&context, options, path_data, archive_writer.get(), output_filename)) {
            error = true;
          }
        }
      } else {
        context.GetDiagnostics()->Error(DiagMessage() << "invalid file path '" << path_data.source << "'");
        error = true;
      }
    }
  }

  if (error) {
    return 1;
  }
  return 0;
}




/**
*	解析
*	aapt::StdErrDiagnostics diagnostics;
*	return aapt::Link(args, &diagnostics);
*/

//解析return aapt::Dump(args);


//解析return aapt::Diff(args);


//解析return aapt::Optimize(args);


//解析	return aapt::PrintVersion();
// DO NOT UPDATE, this is more of a marketing version.
static const char* sMajorVersion = "2";
// Update minor version whenever a feature or flag is added.
static const char* sMinorVersion = "16";

int PrintVersion() {
  std::cerr << "Android Asset Packaging Tool (aapt) " << sMajorVersion << "." << sMinorVersion << std::endl;
  return 0;
}

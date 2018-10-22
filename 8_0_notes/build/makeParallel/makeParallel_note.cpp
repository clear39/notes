//  +@prebuilts/build-tools/$(host_prebuilts)/bin/makeparallel --ninja build/soong/soong_ui.bash --make-mode $(MAKECMDGOALS)


//  @build/make/tools/makeparallel/makeparallel.cpp
int main(int argc, char* argv[]) {
  int in_fd = -1;
  int out_fd = -1;
  bool parallel = false;
  bool keep_going = false;
  bool ninja = false;
  int tokens = 0;

  if (argc > 1 && strcmp(argv[1], "--ninja") == 0) {
    ninja = true;//这里执行
    argv++;
    argc--;
  }

  if (argc < 2) {
    error(EXIT_FAILURE, 0, "expected command to run");
  }

  const char* path = argv[1];         //"build/soong/soong_ui.bash" 因为前面argv++;argc--;
  std::vector<char*> args({argv[1]});

  //这一段，没有实际作用
  std::vector<std::string> makeflags = ReadMakeflags();
  if (ParseMakeflags(makeflags, &in_fd, &out_fd, &parallel, &keep_going)) {
    if (in_fd >= 0 && out_fd >= 0) {
      ......
    }
  }

  std::string jarg;
  if (parallel) {//false
    ......
  }


  if (ninja) {//true
    if (!parallel) {
      // ninja is parallel by default, pass -j1 to disable parallelism if make wasn't parallel
      args.push_back(strdup("-j1"));      //  执行
    } else {
      if (jarg != "") {
        args.push_back(strdup(jarg.c_str()));
      }
    }
    if (keep_going) {//false
      args.push_back(strdup("-k0"));
    }
  } else {
    if (jarg != "") {
      args.push_back(strdup(jarg.c_str()));
    }
  }

  args.insert(args.end(), &argv[2], &argv[argc]); // argv[2] = --make-mode   argv[argc] = $(MAKECMDGOALS)

  args.push_back(nullptr);

  static pid_t pid;

  // Set up signal handlers to forward SIGTERM to child.
  // Assume that all other signals are sent to the entire process group,
  // and that we'll wait for our child to exit instead of handling them.
  struct sigaction action = {};
  action.sa_flags = SA_RESTART;
  action.sa_handler = [](int signal) {
    if (signal == SIGTERM && pid > 0) {
      kill(pid, signal);
    }
  };

  int ret = 0;
  if (!ret) ret = sigaction(SIGHUP, &action, NULL);
  if (!ret) ret = sigaction(SIGINT, &action, NULL);
  if (!ret) ret = sigaction(SIGQUIT, &action, NULL);
  if (!ret) ret = sigaction(SIGTERM, &action, NULL);
  if (!ret) ret = sigaction(SIGALRM, &action, NULL);
  if (ret < 0) {
    error(errno, errno, "sigaction failed");
  }

  pid = fork();
  if (pid < 0) {
    error(errno, errno, "fork failed");
  } else if (pid == 0) {
    // child
    unsetenv("MAKEFLAGS");
    unsetenv("MAKELEVEL");

    // make 3.81 sets the stack ulimit to unlimited, which may cause problems
    // for child processes
    struct rlimit rlim{};
    if (getrlimit(RLIMIT_STACK, &rlim) == 0 && rlim.rlim_cur == RLIM_INFINITY) {
      rlim.rlim_cur = 8*1024*1024;
      setrlimit(RLIMIT_STACK, &rlim);
    }

    int ret = execvp(path, args.data());
    if (ret < 0) {
      error(errno, errno, "exec %s failed", path);
    }
    abort();
  }

  // parent

  siginfo_t status = {};
  int exit_status = 0;
  ret = waitid(P_PID, pid, &status, WEXITED);
  if (ret < 0) {
    error(errno, errno, "waitpid failed");
  } else if (status.si_code == CLD_EXITED) {
    exit_status = status.si_status;
  } else {
    exit_status = -(status.si_status);
  }

  if (tokens > 0) {
    PutJobserverTokens(out_fd, tokens);
  }
  exit(exit_status);
}



static std::vector<std::string> ReadMakeflags() {
  std::vector<std::string> args;

  const char* makeflags_env = getenv("MAKEFLAGS");//由于系统没有该环境变量
  if (makeflags_env == nullptr) {
    return args;//执行这里，直接返回
  }

  // The MAKEFLAGS format is pretty useless.  The first argument might be empty
  // (starts with a leading space), or it might be a set of one-character flags
  // merged together with no leading space, or it might be a variable
  // definition.

  std::string makeflags = makeflags_env;

  // Split makeflags into individual args on spaces.  Multiple spaces are
  // elided, but an initial space will result in a blank arg.
  size_t base = 0;
  size_t found;
  do {
    found = makeflags.find_first_of(" ", base);
    args.push_back(makeflags.substr(base, found - base));
    base = found + 1;
  } while (found != makeflags.npos);

  // Drop the first argument if it is empty
  while (args.size() > 0 && args[0].size() == 0) {
    args.erase(args.begin());
  }

  // Prepend a - to the first argument if it does not have one and is not a
  // variable definition
  if (args.size() > 0 && args[0][0] != '-') {
    if (args[0].find('=') == makeflags.npos) {
      args[0] = '-' + args[0];
    }
  }

  return args;
}


static bool ParseMakeflags(std::vector<std::string>& args,int* in_fd, int* out_fd, bool* parallel, bool* keep_going) {

  std::vector<char*> getopt_argv;
  // getopt starts reading at argv[1]
  getopt_argv.reserve(args.size() + 1);
  getopt_argv.push_back(strdup(""));
  for (std::string& v : args) {
    getopt_argv.push_back(strdup(v.c_str()));
  }

  opterr = 0;
  optind = 1;
  while (1) {
    const static option longopts[] = {
        {"jobserver-fds", required_argument, 0, 0},
        {0, 0, 0, 0},
    };
    int longopt_index = 0;

    int c = getopt_long(getopt_argv.size(), getopt_argv.data(), "kj",longopts, &longopt_index);

    if (c == -1) {
      break;
    }

    switch (c) {
    case 0:
      switch (longopt_index) {
      case 0:
      {
        // jobserver-fds
        if (sscanf(optarg, "%d,%d", in_fd, out_fd) != 2) {
          error(EXIT_FAILURE, 0, "incorrect format for --jobserver-fds: %s", optarg);
        }
        // TODO: propagate in_fd, out_fd
        break;
      }
      default:
        abort();
      }
      break;
    case 'j':
      *parallel = true;
      break;
    case 'k':
      *keep_going = true;
      break;
    case '?':
      // ignore unknown arguments
      break;
    default:
      abort();
    }
  }

  for (char *v : getopt_argv) {
    free(v);
  }

  return true;
}
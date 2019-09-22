//	@	/work/workcodes/aosp-p9.x-auto-alpha/build/soong/cmd/soong_ui/main.go

package main

import (
	"context"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"android/soong/ui/build"
	"android/soong/ui/logger"
	"android/soong/ui/tracer"
)

func indexList(s string, list []string) int {
	for i, l := range list {
		if l == s {
			return i
		}
	}

	return -1
}

func inList(s string, list []string) bool {
	return indexList(s, list) != -1
}


//	# exce "out/soong_ui" “--make-mode -j4“
func main() {
	//	@/work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/logger/logger.go
	/**
	type stdLogger struct {
		stderr  *log.Logger
		verbose bool

		fileLogger *log.Logger
		mutex      sync.Mutex
		file       *os.File
	}

	logger.New(os.Stderr) 主要创建 stdLogger 结构体，并且初始化了stderr *log.Logger 和 fileLogger *log.Logger ；
	其中log库为外部库（不是谷歌android自己实现）
	*/
	log := logger.New(os.Stderr)
	defer log.Cleanup()

	/**
	--make-mode 用于命令make时 命令转换
	--dumpvars-mode 用于设置临时宏变量值
	--dumpvar-mode 用于打印宏定义变量的值
	*/
	if len(os.Args) < 2 || !(inList("--make-mode", os.Args) ||
		os.Args[1] == "--dumpvars-mode" ||
		os.Args[1] == "--dumpvar-mode") {

		log.Fatalln("The `soong` native UI is not yet available.")
	}

	//	
	/**
	context为系统库，
	且 Background 构建一个 默认的Context，
	
	*/
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	//	@/work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/tracer/tracer.go
	/**
	构建
	type tracerImpl struct {
		lock sync.Mutex
		log  logger.Logger

		buf  bytes.Buffer
		file *os.File
		w    io.WriteCloser

		firstEvent bool
		nextTid    uint64
	}
	*/
	trace := tracer.New(log)
	defer trace.Close()

	//	@ /work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/build.go
	/**
	设置信号量处理函数，以及信号出发需要所做的处理
	*/
	build.SetupSignals(log, cancel, func() {
		trace.Close()
		log.Cleanup()
	})

	//	@/work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/context.go
	/***
	构建一个Context变量，并且存储buildCtx
	type Context struct{ *ContextImpl }
	*/
	buildCtx := build.Context{&build.ContextImpl{
		Context:        ctx,
		Logger:         log,
		Tracer:         trace,
		StdioInterface: build.StdioImpl{},
	}}

	// NewConfig	@/work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/config.go
	var config build.Config
	if os.Args[1] == "--dumpvars-mode" || os.Args[1] == "--dumpvar-mode" {
		config = build.NewConfig(buildCtx)
	} else {
		//解析参数，并且根据参数设置构建 config
		config = build.NewConfig(buildCtx, os.Args[1:]...)
	}

	//设置日志打印，可以通过make 命令传参数 showcommands，进行设置
	log.SetVerbose(config.IsVerbose())
	//	@ /work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/build.go
	// 
	build.SetupOutDir(buildCtx, config)

	if config.Dist() {
		logsDir := filepath.Join(config.DistDir(), "logs")
		os.MkdirAll(logsDir, 0777)
		log.SetOutput(filepath.Join(logsDir, "soong.log"))
		trace.SetOutput(filepath.Join(logsDir, "build.trace"))
	} else {
		//默认执行这里
		log.SetOutput(filepath.Join(config.OutDir(), "soong.log"))  // @	out/soong.lgo
		trace.SetOutput(filepath.Join(config.OutDir(), "build.trace"))	//@	out/build.trace
	}

	//查询环境变量 TRACE_BEGIN_SOONG 的值
	if start, ok := os.LookupEnv("TRACE_BEGIN_SOONG"); ok {

		if !strings.HasSuffix(start, "N") {
			if start_time, err := strconv.ParseUint(start, 10, 64); err == nil {
				log.Verbosef("Took %dms to start up.",time.Since(time.Unix(0, int64(start_time))).Nanoseconds()/time.Millisecond.Nanoseconds())
				buildCtx.CompleteTrace("startup", start_time, uint64(time.Now().UnixNano()))
			}
		}

		if executable, err := os.Executable(); err == nil {
			trace.ImportMicrofactoryLog(filepath.Join(filepath.Dir(executable), "."+filepath.Base(executable)+".trace"))
		}
	}

	/**
	NewSourceFinder	@	/work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/finder.go

	*/
	f := build.NewSourceFinder(buildCtx, config)
	defer f.Shutdown()

	//
	build.FindSources(buildCtx, config, f)

	if os.Args[1] == "--dumpvar-mode" {
		dumpVar(buildCtx, config, os.Args[2:])
	} else if os.Args[1] == "--dumpvars-mode" {
		dumpVars(buildCtx, config, os.Args[2:])
	} else {
		//BuildAll = BuildProductConfig | BuildSoong | BuildKati | BuildNinja @/work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/build.go:72:   
		toBuild := build.BuildAll
		if config.Checkbuild() {
			toBuild |= build.RunBuildTests
		}
		//	@ /work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/build.go
		/**
		*	
		*/
		build.Build(buildCtx, config, toBuild)
	}
}


//	@ /work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/build.go

// Build the tree. The 'what' argument can be used to chose which components of
// the build to run.
func Build(ctx Context, config Config, what int = /*BuildProductConfig | BuildSoong | BuildKati | BuildNinja*/) {
	// 在soong.log文件中 打印 参数
	ctx.Verboseln("Starting build with args:", config.Arguments())

	//在soong.log文件中输入日志（环境变量）
	ctx.Verboseln("Environment:", config.Environment().Environ())

	if config.SkipMake() {  //false
		ctx.Verboseln("Skipping Make/Kati as requested")
		what = what & (BuildSoong | BuildNinja)
	}

	if inList("help", config.Arguments()) {
		//	@ /work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/build.go
		help(ctx, config, what)  //执行了 build/make/help.sh 脚本，输出帮助信息  
		return
	} else if inList("clean", config.Arguments()) || inList("clobber", config.Arguments()) {
		//	@ /work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/cleanbuild.go
		clean(ctx, config, what)	//删除out目录下所有目录和文件，但不删除out目录
		return
	}

	// Make sure that no other Soong process is running with the same output directory
	// 这里主要是加锁，防止多个终端编译
	// 这里会在out目录下生成一个.lock文件，然后打开该文件，并且加锁
	buildLock := BecomeSingletonOrFail(ctx, config)	//	@ /work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/proc_sync.go
	defer buildLock.Unlock()

	// /work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/build.go

	//SetupOutDir 再次确认out目录是否存在，且判断 Android.mk 、CleanSpec.mk 、.soong.in_make、ninja_build、.out-dir 是否存在，不存在则创建
	SetupOutDir(ctx, config)

	// @ /work/workcodes/aosp-p9.x-auto-alpha/buildsoong/ui/build/build.go:75
	// 这里判断当前系统是否区分大小写
	// 主要往 out目录下 casecheck.txt 和 CaseCheck.txt 分别写入 “a” 和 “B” ，然后 读出  casecheck.txt 的字符，再与原来值比较
	checkCaseSensitivity(ctx, config)

	// @ /work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/util.go
	//  config.TempDir() 返回目录数组，
	// ensureEmptyDirectoriesExist 主要是删除并重新创建所有目录
	ensureEmptyDirectoriesExist(ctx, config.TempDir()) // TempDir为 out/soong/.temp/ (注意这里是目录)


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if what&BuildProductConfig != 0 { // true
		// Run make for product config
		//	@ /work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/dumpvars.go
		// 这里通过 prebuilts/build-tools/linux-x86/bin/ckati 工具将 build/make/core/config.mk 的宏变量值读入系统
		// 同时通过 config 的  
		runMakeProductConfig(ctx, config) // 进入dumpvars-note.go 分析 可以通过 修改 Banner 将 所有参数打印控制台
	}

	if inList("installclean", config.Arguments()) {
		//	@ /work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/cleanbuild.go
		installClean(ctx, config, what)
		ctx.Println("Deleted images and staging directories.")
		return
	} else if inList("dataclean", config.Arguments()) {
		//	@ /work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/cleanbuild.go
		dataClean(ctx, config, what)
		ctx.Println("Deleted data files.")
		return
	}

	if what&BuildSoong != 0 { // true
		//	@ /work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/soong.go
		// Run Soong
		runSoong(ctx, config)
	}

	if what&BuildKati != 0 { // true
		//	@ /work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/kati.go
		// Run ckati
		runKati(ctx, config)

		ioutil.WriteFile(config.LastKatiSuffixFile(), []byte(config.KatiSuffix()), 0777)
	} else {
		// Load last Kati Suffix if it exists
		if katiSuffix, err := ioutil.ReadFile(config.LastKatiSuffixFile()); err == nil {
			ctx.Verboseln("Loaded previous kati config:", string(katiSuffix))
			config.SetKatiSuffix(string(katiSuffix))
		}
	}

	//	@/work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/build.go
	// Write combined ninja file
	createCombinedBuildNinjaFile(ctx, config)

	if what&RunBuildTests != 0 { // false
		testForDanglingRules(ctx, config)
	}

	if what&BuildNinja != 0 { // true
		if !config.SkipMake() {
			installCleanIfNecessary(ctx, config)
		}
		//@  /work/workcodes/aosp-p9.x-auto-alpha/build/soong/ui/build/ninja.go
		// Run ninja
		runNinja(ctx, config)
	}
}






























func dumpVar(ctx build.Context, config build.Config, args []string) {
	flags := flag.NewFlagSet("dumpvar", flag.ExitOnError)
	flags.Usage = func() {
		fmt.Fprintf(os.Stderr, "usage: %s --dumpvar-mode [--abs] <VAR>\n\n", os.Args[0])
		fmt.Fprintln(os.Stderr, "In dumpvar mode, print the value of the legacy make variable VAR to stdout")
		fmt.Fprintln(os.Stderr, "")

		fmt.Fprintln(os.Stderr, "'report_config' is a special case that prints the human-readable config banner")
		fmt.Fprintln(os.Stderr, "from the beginning of the build.")
		fmt.Fprintln(os.Stderr, "")
		flags.PrintDefaults()
	}
	abs := flags.Bool("abs", false, "Print the absolute path of the value")
	flags.Parse(args)

	if flags.NArg() != 1 {
		flags.Usage()
		os.Exit(1)
	}

	varName := flags.Arg(0)
	if varName == "report_config" {
		varData, err := build.DumpMakeVars(ctx, config, nil, build.BannerVars)
		if err != nil {
			ctx.Fatal(err)
		}

		fmt.Println(build.Banner(varData))
	} else {
		varData, err := build.DumpMakeVars(ctx, config, nil, []string{varName})
		if err != nil {
			ctx.Fatal(err)
		}

		if *abs {
			var res []string
			for _, path := range strings.Fields(varData[varName]) {
				if abs, err := filepath.Abs(path); err == nil {
					res = append(res, abs)
				} else {
					ctx.Fatalln("Failed to get absolute path of", path, err)
				}
			}
			fmt.Println(strings.Join(res, " "))
		} else {
			fmt.Println(varData[varName])
		}
	}
}

func dumpVars(ctx build.Context, config build.Config, args []string) {
	flags := flag.NewFlagSet("dumpvars", flag.ExitOnError)
	flags.Usage = func() {
		fmt.Fprintf(os.Stderr, "usage: %s --dumpvars-mode [--vars=\"VAR VAR ...\"]\n\n", os.Args[0])
		fmt.Fprintln(os.Stderr, "In dumpvars mode, dump the values of one or more legacy make variables, in")
		fmt.Fprintln(os.Stderr, "shell syntax. The resulting output may be sourced directly into a shell to")
		fmt.Fprintln(os.Stderr, "set corresponding shell variables.")
		fmt.Fprintln(os.Stderr, "")

		fmt.Fprintln(os.Stderr, "'report_config' is a special case that dumps a variable containing the")
		fmt.Fprintln(os.Stderr, "human-readable config banner from the beginning of the build.")
		fmt.Fprintln(os.Stderr, "")
		flags.PrintDefaults()
	}

	varsStr := flags.String("vars", "", "Space-separated list of variables to dump")
	absVarsStr := flags.String("abs-vars", "", "Space-separated list of variables to dump (using absolute paths)")

	varPrefix := flags.String("var-prefix", "", "String to prepend to all variable names when dumping")
	absVarPrefix := flags.String("abs-var-prefix", "", "String to prepent to all absolute path variable names when dumping")

	flags.Parse(args)

	if flags.NArg() != 0 {
		flags.Usage()
		os.Exit(1)
	}

	vars := strings.Fields(*varsStr)
	absVars := strings.Fields(*absVarsStr)

	allVars := append([]string{}, vars...)
	allVars = append(allVars, absVars...)

	if i := indexList("report_config", allVars); i != -1 {
		allVars = append(allVars[:i], allVars[i+1:]...)
		allVars = append(allVars, build.BannerVars...)
	}

	if len(allVars) == 0 {
		return
	}

	varData, err := build.DumpMakeVars(ctx, config, nil, allVars)
	if err != nil {
		ctx.Fatal(err)
	}

	for _, name := range vars {
		if name == "report_config" {
			fmt.Printf("%sreport_config='%s'\n", *varPrefix, build.Banner(varData))
		} else {
			fmt.Printf("%s%s='%s'\n", *varPrefix, name, varData[name])
		}
	}
	for _, name := range absVars {
		var res []string
		for _, path := range strings.Fields(varData[name]) {
			abs, err := filepath.Abs(path)
			if err != nil {
				ctx.Fatalln("Failed to get absolute path of", path, err)
			}
			res = append(res, abs)
		}
		fmt.Printf("%s%s='%s'\n", *absVarPrefix, name, strings.Join(res, " "))
	}
}

//这个文件是由 "build/blueprint/microfactory/microfactory.go" 替换而来 
// @ out/.microfactory_Linux_intermediates/src/microfactory.go`

/**
	GOROOT = /work/workcodes/aosp-p9.x-auto-alpha/prebuilts/go/linux-x86
	mf_cmd = prebuilts/go/linux-x86/bin/go run out/.microfactory_Linux_intermediates/src/microfactory.go
	mf_bin = out/microfactory_Linux
	BLUEPRINTDIR = build/blueprint/
	SRCDIR = /work/workcodes/aosp-p9.x-auto-alpha/
	EXTRA_ARGS = -pkg-path android/soong=/work/workcodes/aosp-p9.x-auto-alpha/build/soong
	built_bin = out/soong_ui
	$2 = android/soong/cmd/soong_ui

	

	GOROOT=$(cd $GOROOT; pwd) ${mf_cmd} -b "${mf_bin}" \
    ┊   ┊   -pkg-path "github.com/google/blueprint=${BLUEPRINTDIR}" \
    ┊   ┊   -trimpath "${SRCDIR}" \
    ┊   ┊   ${EXTRA_ARGS} \
	┊   ┊   -o "${built_bin}" $2

	
	prebuilts/go/linux-x86/bin/go run out/.microfactory_Linux_intermediates/src/microfactory.go 
	-b out/microfactory_Linux
	-pkg-path "github.com/google/blueprint=build/blueprint/"
	-trimpath "work/workcodes/aosp-p9.x-auto-alpha/"
	-pkg-path android/soong=/work/workcodes/aosp-p9.x-auto-alpha/build/soong
	-o out/soong_ui 
	android/soong/cmd/soong_ui

*/

/**
第一行代码 package main 定义了包名。你必须在源文件中非注释的第一行指明这个文件属于哪个包，
如：package main表示一个可独立执行的程序，每个 Go 应用程序都包含一个名为 main 的包
*/
package main 

import (
	"bytes"
	"crypto/sha1"
	"flag"		//	
	"fmt"		// 告诉 Go 编译器这个程序需要使用 fmt 包（的函数，或其他元素），fmt 包实现了格式化 IO（输入/输出）的函数
	"go/ast"
	"go/build"
	"go/parser"
	"go/token"
	"io"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"
)

var (
	goToolDir = filepath.Join(runtime.GOROOT(), "pkg", "tool", runtime.GOOS+"_"+runtime.GOARCH)
	goVersion = findGoVersion()
	isGo18    = strings.Contains(goVersion, "go1.8")
)

func findGoVersion() string {
	if version, err := ioutil.ReadFile(filepath.Join(runtime.GOROOT(), "VERSION")); err == nil {
		return string(version)
	}

	cmd := exec.Command(filepath.Join(runtime.GOROOT(), "bin", "go"), "version")
	if version, err := cmd.Output(); err == nil {
		return string(version)
	} else {
		panic(fmt.Sprintf("Unable to discover go version: %v", err))
	}
}

type Config struct {
	Race    bool
	Verbose bool

	TrimPath string

	TraceFunc func(name string) func()

	LogFunc func(name string) func()

	pkgs  []string
	paths map[string]string
}

func (c *Config) Map(pkgPrefix, pathPrefix string) error {
	if c.paths == nil {
		c.paths = make(map[string]string)
	}
	if _, ok := c.paths[pkgPrefix]; ok {
		return fmt.Errorf("Duplicate package prefix: %q", pkgPrefix)
	}

	c.pkgs = append(c.pkgs, pkgPrefix)
	c.paths[pkgPrefix] = pathPrefix

	return nil
}

// Path takes a package name, applies the path mappings and returns the resulting path.
//
// If the package isn't mapped, we'll return false to prevent compilation attempts.
func (c *Config) Path(pkg string) (string, bool, error) {
	if c == nil || c.paths == nil {
		return "", false, fmt.Errorf("No package mappings")
	}

	for _, pkgPrefix := range c.pkgs {
		if pkg == pkgPrefix {
			return c.paths[pkgPrefix], true, nil
		} else if strings.HasPrefix(pkg, pkgPrefix+"/") {
			return filepath.Join(c.paths[pkgPrefix], strings.TrimPrefix(pkg, pkgPrefix+"/")), true, nil
		}
	}

	return "", false, nil
}

func (c *Config) trace(format string, a ...interface{}) func() {
	if c.TraceFunc == nil {
		return func() {}
	}
	s := strings.TrimSpace(fmt.Sprintf(format, a...))
	return c.TraceFunc(s)
}

func un(f func()) {
	f()
}

type GoPackage struct {
	Name string

	// Inputs
	directDeps []*GoPackage // specified directly by the module
	allDeps    []*GoPackage // direct dependencies and transitive dependencies
	files      []string

	// Outputs
	pkgDir     string
	output     string
	hashResult []byte

	// Status
	mutex    sync.Mutex
	compiled bool
	failed   error
	rebuilt  bool
}

// LinkedHashMap<string, GoPackage>
type linkedDepSet struct {
	packageSet  map[string](*GoPackage)
	packageList []*GoPackage
}

func newDepSet() *linkedDepSet {
	return &linkedDepSet{packageSet: make(map[string]*GoPackage)}
}
func (s *linkedDepSet) tryGetByName(name string) (*GoPackage, bool) {
	pkg, contained := s.packageSet[name]
	return pkg, contained
}
func (s *linkedDepSet) getByName(name string) *GoPackage {
	pkg, _ := s.tryGetByName(name)
	return pkg
}
func (s *linkedDepSet) add(name string, goPackage *GoPackage) {
	s.packageSet[name] = goPackage
	s.packageList = append(s.packageList, goPackage)
}
func (s *linkedDepSet) ignore(name string) {
	s.packageSet[name] = nil
}

// FindDeps searches all applicable go files in `path`, parses all of them
// for import dependencies that exist in pkgMap, then recursively does the
// same for all of those dependencies.
func (p *GoPackage) FindDeps(config *Config, path string) error {
	defer un(config.trace("findDeps"))

	depSet := newDepSet()
	err := p.findDeps(config, path, depSet)
	if err != nil {
		return err
	}
	p.allDeps = depSet.packageList
	return nil
}

// Roughly equivalent to go/build.Context.match
func matchBuildTag(name string) bool {
	if name == "" {
		return false
	}
	if i := strings.Index(name, ","); i >= 0 {
		ok1 := matchBuildTag(name[:i])
		ok2 := matchBuildTag(name[i+1:])
		return ok1 && ok2
	}
	if strings.HasPrefix(name, "!!") {
		return false
	}
	if strings.HasPrefix(name, "!") {
		return len(name) > 1 && !matchBuildTag(name[1:])
	}

	if name == runtime.GOOS || name == runtime.GOARCH || name == "gc" {
		return true
	}
	for _, tag := range build.Default.BuildTags {
		if tag == name {
			return true
		}
	}
	for _, tag := range build.Default.ReleaseTags {
		if tag == name {
			return true
		}
	}

	return false
}

func parseBuildComment(comment string) (matches, ok bool) {
	if !strings.HasPrefix(comment, "//") {
		return false, false
	}
	for i, c := range comment {
		if i < 2 || c == ' ' || c == '\t' {
			continue
		} else if c == '+' {
			f := strings.Fields(comment[i:])
			if f[0] == "+build" {
				matches = false
				for _, tok := range f[1:] {
					matches = matches || matchBuildTag(tok)
				}
				return matches, true
			}
		}
		break
	}
	return false, false
}

// findDeps is the recursive(递归的) version of FindDeps. allPackages is the map of
// all locally defined packages so that the same dependency of two different
// packages is only resolved once.
func (p *GoPackage) findDeps(config *Config, path string, allPackages *linkedDepSet) error {
	// If this ever becomes too slow, we can look at reading the files once instead of twice
	// But that just complicates things today, and we're already really fast.
	foundPkgs, err := parser.ParseDir(token.NewFileSet(), path, func(fi os.FileInfo) bool {
		name := fi.Name()
		if fi.IsDir() || strings.HasSuffix(name, "_test.go") || name[0] == '.' || name[0] == '_' {
			return false
		}
		if runtime.GOOS != "darwin" && strings.HasSuffix(name, "_darwin.go") {
			return false
		}
		if runtime.GOOS != "linux" && strings.HasSuffix(name, "_linux.go") {
			return false
		}
		return true
	}, parser.ImportsOnly|parser.ParseComments)
	if err != nil {
		return fmt.Errorf("Error parsing directory %q: %v", path, err)
	}

	var foundPkg *ast.Package
	// foundPkgs is a map[string]*ast.Package, but we only want one package
	if len(foundPkgs) != 1 {
		return fmt.Errorf("Expected one package in %q, got %d", path, len(foundPkgs))
	}

	
	// Extract the first (and only) entry from the map.
	for _, pkg := range foundPkgs {
		foundPkg = pkg
	}

	var deps []string
	localDeps := make(map[string]bool)

	for filename, astFile := range foundPkg.Files {
		ignore := false
		for _, commentGroup := range astFile.Comments {
			for _, comment := range commentGroup.List {
				if matches, ok := parseBuildComment(comment.Text); ok && !matches {
					ignore = true
				}
			}
		}
		if ignore {
			continue
		}

		p.files = append(p.files, filename)

		for _, importSpec := range astFile.Imports {
			name, err := strconv.Unquote(importSpec.Path.Value)
			if err != nil {
				return fmt.Errorf("%s: invalid quoted string: <%s> %v", filename, importSpec.Path.Value, err)
			}

			//在allPackages中通过键值name查找对应的GoPackage，如果已经存在，则添加
			if pkg, ok := allPackages.tryGetByName(name); ok {
				if pkg != nil {
					if _, ok := localDeps[name]; !ok {
						deps = append(deps, name)
						localDeps[name] = true
					}
				}
				continue
			}

			var pkgPath string
			//这里通过config.Path方法，进行前缀过滤和拼接得到对应的路径，并且返回给path
			if path, ok, err := config.Path(name); err != nil {
				return err
			} else if !ok {
				// Probably in the stdlib, but if not, then the compiler will fail with a reasonable error message
				// Mark it as such so that we don't try to decode its path again.
				allPackages.ignore(name)
				continue
			} else {
				pkgPath = path
			}

			pkg := &GoPackage{
				Name: name,
			}
			deps = append(deps, name)
			allPackages.add(name, pkg)
			localDeps[name] = true

			if err := pkg.findDeps(config, pkgPath, allPackages); err != nil {
				return err
			}
		}
	}

	sort.Strings(p.files)

	if config.Verbose {
		fmt.Fprintf(os.Stderr, "Package %q depends on %v\n", p.Name, deps)
	}

	sort.Strings(deps)
	for _, dep := range deps {
		p.directDeps = append(p.directDeps, allPackages.getByName(dep))
	}
	return nil
}

func (p *GoPackage) Compile(config *Config, outDir string) error {
	p.mutex.Lock()
	defer p.mutex.Unlock()
	if p.compiled {
		return p.failed
	}
	p.compiled = true

	// Build all dependencies in parallel, then fail if any of them failed.
	var wg sync.WaitGroup
	for _, dep := range p.directDeps { // directDeps 为 GoPackage 切片（数组）
		wg.Add(1)
		go func(dep *GoPackage) {
			defer wg.Done()
			dep.Compile(config, outDir) //递归调用
		}(dep)
	}
	wg.Wait()
	for _, dep := range p.directDeps {
		if dep.failed != nil {
			p.failed = dep.failed
			return p.failed
		}
	}

	endTrace := config.trace("check compile %s", p.Name)

	p.pkgDir = filepath.Join(outDir, strings.Replace(p.Name, "/", "-", -1))
	p.output = filepath.Join(p.pkgDir, p.Name) + ".a" //
	shaFile := p.output + ".hash"   //

	hash := sha1.New()
	fmt.Fprintln(hash, runtime.GOOS, runtime.GOARCH, goVersion)


	/**
	/work/workcodes/aosp-p9.x-auto-alpha/prebuilts/go/linux-x86/pkg/tool/linux_amd64/compile 
	-o 
	-p 
	-complete
	-pack
	-nolocalimports
	-trimpath  /work/workcodes/aosp-p9.x-auto-alpha
	-I 
	*/
	cmd := exec.Command(filepath.Join(goToolDir, "compile"),
		"-o", p.output,
		"-p", p.Name,
		"-complete", "-pack", "-nolocalimports")

	
	
	if !isGo18 { //false
		cmd.Args = append(cmd.Args, "-c", fmt.Sprintf("%d", runtime.NumCPU()))
	}
	if config.Race { //false
		cmd.Args = append(cmd.Args, "-race")
		fmt.Fprintln(hash, "-race")
	}
	if config.TrimPath != "" {
		cmd.Args = append(cmd.Args, "-trimpath", config.TrimPath)
		fmt.Fprintln(hash, config.TrimPath)
	}
	for _, dep := range p.directDeps {
		cmd.Args = append(cmd.Args, "-I", dep.pkgDir)
		hash.Write(dep.hashResult)
	}
	for _, filename := range p.files {
		cmd.Args = append(cmd.Args, filename)
		fmt.Fprintln(hash, filename)

		// Hash the contents of the input files
		f, err := os.Open(filename)
		if err != nil {
			f.Close()
			err = fmt.Errorf("%s: %v", filename, err)
			p.failed = err
			return err
		}
		_, err = io.Copy(hash, f)
		if err != nil {
			f.Close()
			err = fmt.Errorf("%s: %v", filename, err)
			p.failed = err
			return err
		}
		f.Close()
	}
	p.hashResult = hash.Sum(nil)

	var rebuild bool
	if _, err := os.Stat(p.output); err != nil {
		rebuild = true
	}
	if !rebuild {
		if oldSha, err := ioutil.ReadFile(shaFile); err == nil {
			rebuild = !bytes.Equal(oldSha, p.hashResult)
		} else {
			rebuild = true
		}
	}

	endTrace()
	if !rebuild {
		return nil
	}
	defer un(config.trace("compile %s", p.Name))

	err := os.RemoveAll(p.pkgDir)
	if err != nil {
		err = fmt.Errorf("%s: %v", p.Name, err)
		p.failed = err
		return err
	}

	err = os.MkdirAll(filepath.Dir(p.output), 0777)
	if err != nil {
		err = fmt.Errorf("%s: %v", p.Name, err)
		p.failed = err
		return err
	}

	cmd.Stdin = nil
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if config.Verbose {
		fmt.Fprintln(os.Stderr, cmd.Args)
	}
	err = cmd.Run()
	if err != nil {
		commandText := strings.Join(cmd.Args, " ")
		err = fmt.Errorf("%q: %v", commandText, err)
		p.failed = err
		return err
	}

	err = ioutil.WriteFile(shaFile, p.hashResult, 0666)
	if err != nil {
		err = fmt.Errorf("%s: %v", p.Name, err)
		p.failed = err
		return err
	}

	p.rebuilt = true

	return nil
}

func (p *GoPackage) Link(config *Config, out string) error {
	if p.Name != "main" {
		return fmt.Errorf("Can only link main package")
	}
	endTrace := config.trace("check link %s", p.Name)

	// 
	shaFile := filepath.Join(filepath.Dir(out), "."+filepath.Base(out)+"_hash")

	if !p.rebuilt {
		if _, err := os.Stat(out); err != nil {
			p.rebuilt = true
		} else if oldSha, err := ioutil.ReadFile(shaFile); err != nil {
			p.rebuilt = true
		} else {
			p.rebuilt = !bytes.Equal(oldSha, p.hashResult)
		}
	}
	endTrace()
	if !p.rebuilt {
		return nil
	}
	defer un(config.trace("link %s", p.Name))

	err := os.Remove(shaFile)
	if err != nil && !os.IsNotExist(err) {
		return err
	}
	err = os.Remove(out)
	if err != nil && !os.IsNotExist(err) {
		return err
	}

	/**
	/work/workcodes/aosp-p9.x-auto-alpha/prebuilts/go/linux-x86/pkg/tool/linux_amd64/link 
	-o 
	-L [directory]   //add specified directory to library path
	*/

	cmd := exec.Command(filepath.Join(goToolDir, "link"), "-o", out)
	if config.Race {
		cmd.Args = append(cmd.Args, "-race") //enable race detector
	}

	
	for _, dep := range p.allDeps {
		cmd.Args = append(cmd.Args, "-L", dep.pkgDir)
	}
	cmd.Args = append(cmd.Args, p.output)



	cmd.Stdin = nil
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if config.Verbose {
		fmt.Fprintln(os.Stderr, cmd.Args)
	}
	err = cmd.Run()
	if err != nil {
		return fmt.Errorf("command %s failed with error %v", cmd.Args, err)
	}

	return ioutil.WriteFile(shaFile, p.hashResult, 0666)
}


// Path takes a package name, applies the path mappings and returns the resulting path.
//
// If the package isn't mapped, we'll return false to prevent compilation attempts.
func (c *Config) Path(pkg string) (string, bool, error) {
	if c == nil || c.paths == nil {
		return "", false, fmt.Errorf("No package mappings")
	}

	/*
	-pkg-path "github.com/google/blueprint=build/blueprint/"
	-pkg-path "android/soong=/work/workcodes/aosp-p9.x-auto-alpha/build/soong"
	*/
	for _, pkgPrefix := range c.pkgs {
		if pkg == pkgPrefix {
			return c.paths[pkgPrefix], true, nil
		} else if strings.HasPrefix(pkg, pkgPrefix+"/") {
			return filepath.Join(c.paths[pkgPrefix], strings.TrimPrefix(pkg, pkgPrefix+"/")), true, nil
		}
	}

	return "", false, nil
}


func Build(config *Config, out, pkg string) (*GoPackage, error) {
	p := &GoPackage{
		Name: "main",
	}

	lockFileName := filepath.Join(filepath.Dir(out), "."+filepath.Base(out)+".lock")
	lockFile, err := os.OpenFile(lockFileName, os.O_RDWR|os.O_CREATE, 0666)
	if err != nil {
		return nil, fmt.Errorf("Error creating lock file (%q): %v", lockFileName, err)
	}
	defer lockFile.Close()

	err = syscall.Flock(int(lockFile.Fd()), syscall.LOCK_EX)
	if err != nil {
		return nil, fmt.Errorf("Error locking file (%q): %v", lockFileName, err)
	}

	//这里匹配查找字符串 path= “build/blueprint/”
	path, ok, err := config.Path(pkg)   //pkg = "github.com/google/blueprint/microfactory/main"
	if err != nil {
		return nil, fmt.Errorf("Error finding package %q for main: %v", pkg, err)
	}
	if !ok {
		return nil, fmt.Errorf("Could not find package %q", pkg)
	}

	//	intermediates = "/work/workcodes/aosp-p9.x-auto-alpha/out/.microfactory_Linux_intermediates"
	intermediates := filepath.Join(filepath.Dir(out), "."+filepath.Base(out)+"_intermediates")
	if err := os.MkdirAll(intermediates, 0777); err != nil {
		return nil, fmt.Errorf("Failed to create intermediates directory: %v", err)
	}

	//
	if err := p.FindDeps(config, path); err != nil {//	path= “build/blueprint/”
		return nil, fmt.Errorf("Failed to find deps of %v: %v", pkg, err)
	}

	//
	if err := p.Compile(config, intermediates); err != nil {//	intermediates = "out/.microfactory_Linux_intermediates"
		return nil, fmt.Errorf("Failed to compile %v: %v", pkg, err)
	}

	//
	if err := p.Link(config, out); err != nil {// out
		return nil, fmt.Errorf("Failed to link %v: %v", pkg, err)
	}
	return p, nil
}

// rebuildMicrofactory checks to see if microfactory itself needs to be rebuilt,
// and if does, it will launch a new copy and return true. Otherwise it will return
// false to continue executing.
func rebuildMicrofactory(config *Config, mybin string) bool {
	if pkg, err := Build(config, mybin /*="out/microfactory_Linux"*/, "github.com/google/blueprint/microfactory/main"); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	} else if !pkg.rebuilt {
		return false
	}

	cmd := exec.Command(mybin, os.Args[1:]...)
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err == nil {
		return true
	} else if e, ok := err.(*exec.ExitError); ok {
		os.Exit(e.ProcessState.Sys().(syscall.WaitStatus).ExitStatus())
	}
	os.Exit(1)
	return true
}

/*
prebuilts/go/linux-x86/bin/go run out/.microfactory_Linux_intermediates/src/microfactory.go 
-b /work/workcodes/aosp-p9.x-auto-alpha/out/microfactory_Linux
-pkg-path "github.com/google/blueprint=build/blueprint/"
-trimpath "work/workcodes/aosp-p9.x-auto-alpha/"
-pkg-path "android/soong=/work/workcodes/aosp-p9.x-auto-alpha/build/soong"
-o /work/workcodes/aosp-p9.x-auto-alpha/out/soong_ui android/soong/cmd/soong_ui
*/

// microfactory.bash will make a copy of this file renamed into the main package for use with `go run`

/**
 func main() 是程序开始执行的函数。main 函数是每一个可执行程序所必须包含的，
 一般来说都是在启动后第一个执行的函数（如果有 init() 函数则会先执行该函数）。
*/



/**
如果 /work/workcodes/aosp-p9.x-auto-alpha/out/microfactory_Linux 执行文件存在？？？
*/

func main() { Main() }
func Main() {
	var output, mybin string
	var config Config
	// 构造一个pkgPathMappingVar并且赋值给 pkgMap （这个pkgMap 必须没有申明）
	pkgMap := pkgPathMappingVar{&config}

	//通过flag包创建一个flags类进行参数解析
	flags := flag.NewFlagSet("", flag.ExitOnError)
	flags.BoolVar(&config.Race, "race", false, "enable data race detection.")
	flags.BoolVar(&config.Verbose, "v", false, "Verbose")
	flags.StringVar(&output, "o", "", "Output file")
	flags.StringVar(&mybin, "b", "", "Microfactory binary location")
	flags.StringVar(&config.TrimPath, "trimpath", "", "remove prefix from recorded source file paths")

	flags.Var(&pkgMap, "pkg-path", "Mapping of package prefixes to file paths")


	//解析输入的参数
	err := flags.Parse(os.Args[1:])
	

	if err == flag.ErrHelp || flags.NArg() != 1 || output == "" {
		fmt.Fprintln(os.Stderr, "Usage:", os.Args[0], "-o out/binary <main-package>")
		flags.PrintDefaults()
		os.Exit(1)
	}

	//tracePath = “out/.soong_ui.trace”
	tracePath := filepath.Join(filepath.Dir(output), "."+filepath.Base(output)+".trace")

	

	//用于保存编译工具log日志信息，可以用于调试
	if traceFile, err := os.OpenFile(tracePath, os.O_RDWR|os.O_CREATE|os.O_APPEND, 0666); err == nil {
		defer traceFile.Close()

		//这里用于函数调用轨迹
		config.TraceFunc = func(name string) func() {
			fmt.Fprintf(traceFile, "%d Bigen %s\n", time.Now().UnixNano()/1000, name)
			return func() {
				fmt.Fprintf(traceFile, "%d End %s\n", time.Now().UnixNano()/1000, name)
			}
		}
	}
	if executable, err := os.Executable(); err == nil {
		//	defer语句 https://www.jianshu.com/p/5b0b36f398a2

		// 	executable = "/work/workcodes/aosp-p9.x-auto-alpha/out/microfactory_Linux"
		defer un(config.trace("microfactory %s", executable))  //un函数内部定义（执行传入参数函数）
	} else {
		defer un(config.trace("microfactory <unknown>"))
	}

	if mybin != "" { //mybin 不为空
		if rebuildMicrofactory(&config, mybin) {
			return//执行成功，则直接返回
		}
	}

	if _, err := Build(&config, output, flags.Arg(0)); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

// pkgPathMapping can be used with flag.Var to parse -pkg-path arguments of
// <package-prefix>=<path-prefix> mappings.
type pkgPathMappingVar struct{ *Config }

/**
String 方法 是属于pkgPathMappingVar 结构体的
*/
func (pkgPathMappingVar) String() string {
	return "<package-prefix>=<path-prefix>"
}


/**
Set 方法是所有pkgPathMappingVar 结构体的，其中 *pkgPathMappingVar 为引用传递
*/
func (p *pkgPathMappingVar) Set(value string) error {	
	equalPos := strings.Index(value, "=")
	if equalPos == -1 {
		return fmt.Errorf("Argument must be in the form of: %q", p.String())
	}

	pkgPrefix := strings.TrimSuffix(value[:equalPos], "/")
	pathPrefix := strings.TrimSuffix(value[equalPos+1:], "/")

	return p.Map(pkgPrefix, pathPrefix)
}


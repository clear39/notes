import (
	"io/ioutil"
	"os"
	"path/filepath"
)

/**

//	@	build/soong/ui/build/finder.go

f := build.NewSourceFinder(buildCtx, config)
defer f.Shutdown()

//	@	build/soong/ui/build/finder.go
build.FindSources(buildCtx, config, f)

*/

///////////////////////////////////////////////////////////////////////////////////////////////////////
//	@	build/soong/ui/build/finder.go
func NewSourceFinder(ctx Context, config Config) (f *finder.Finder) {
	ctx.BeginTrace("find modules")
	defer ctx.EndTrace()

	dir, err := os.Getwd()
	if err != nil {
		ctx.Fatalf("No working directory for module-finder: %v", err.Error())
	}
	filesystem := fs.OsFs

	// if the root dir is ignored, then the subsequent error messages are very confusing,
	// so check for that upfront
	pruneFiles := []string{".out-dir", ".find-ignore"}
	for _, name := range pruneFiles {
		prunePath := filepath.Join(dir, name) // out/ + name
		_, statErr := filesystem.Lstat(prunePath)
		if statErr == nil {
			ctx.Fatalf("%v must not exist", prunePath)
		}
	}
	/*

	 */
	cacheParams := finder.CacheParams{
		WorkingDirectory: dir,
		RootDirs:         []string{"."},
		ExcludeDirs:      []string{".git", ".repo"},
		PruneFiles:       pruneFiles, // {".out-dir", ".find-ignore"}
		IncludeFiles:     []string{"Android.mk", "Android.bp", "Blueprints", "CleanSpec.mk", "TEST_MAPPING"},
	}
	dumpDir := config.FileListDir() // out/.module_paths
	f, err = finder.New(cacheParams, filesystem, logger.New(ioutil.Discard),
		filepath.Join(dumpDir, "files.db"))
	if err != nil {
		ctx.Fatalf("Could not create module-finder: %v", err)
	}
	return f
}

/**
@	\build/soong/finder/finder.go
*/
// a CacheParams specifies which files and directories the user wishes be scanned and
// potentially added to the cache
type CacheParams struct {
	// WorkingDirectory is used as a base for any relative file paths given to the Finder
	WorkingDirectory string

	// RootDirs are the root directories used to initiate the search
	RootDirs []string

	// ExcludeDirs are directory names that if encountered are removed from the search
	ExcludeDirs []string

	// PruneFiles are file names that if encountered prune their entire directory
	// (including siblings)
	PruneFiles []string

	// IncludeFiles are file names to include as matches
	IncludeFiles []string
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
import (
	"errors"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"os"
	"sync"
)

/**

	log := logger.New(os.Stderr)
	defer log.Cleanup()


**/

//	@	build/soong/ui/logger/logger.go

type stdLogger struct {
	stderr  *log.Logger
	verbose bool

	fileLogger *log.Logger
	mutex      sync.Mutex
	file       *os.File
}

// New creates a new Logger. The out variable sets the destination, commonly
// os.Stderr, but it may be a buffer for tests, or a separate log file if
// the user doesn't need to see the output.
func New(out io.Writer) *stdLogger {
	return &stdLogger{
		stderr:     log.New(out, "", log.Ltime),
		fileLogger: log.New(ioutil.Discard, "", log.Ldate|log.Lmicroseconds|log.Llongfile),
	}
}

// Cleanup should be used with defer in your main function. It will close the
// log file and convert any Fatal panics back to os.Exit(1)
func (s *stdLogger) Cleanup() {
	fatal := false
	p := recover()

	if _, ok := p.(fatalLog); ok {
		fatal = true
		p = nil
	} else if p != nil {
		s.Println(p)
	}

	s.Close()

	if p != nil {
		panic(p)
	} else if fatal {
		os.Exit(1)
	}
}

// Fatalln is equivalent to Println() followed by a call to panic() that
// Cleanup will convert to a os.Exit(1).
func (s *stdLogger) Fatalln(v ...interface{}) {
	output := fmt.Sprintln(v...)
	s.Output(2, output)
	panic(fatalLog(errors.New(output)))
}

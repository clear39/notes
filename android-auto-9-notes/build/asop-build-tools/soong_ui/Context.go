import "context"

/*
	buildCtx := build.Context{&build.ContextImpl{
		Context:        ctx,
		Logger:         log,
		Tracer:         trace,
		StdioInterface: build.StdioImpl{},
	}}
*/
//	@	build/soong/ui/build/context.go

/*
Context 结构体 继承 *ContextImpl
*/
type Context struct{ *ContextImpl }

/*
Context 结构体 继承 context.Context 和 logger.Logger，StdioInterface
另外成员有 Thread tracer.Thread 和 Tracer tracer.Tracer

*/
type ContextImpl struct {
	context.Context
	logger.Logger

	StdioInterface

	Thread tracer.Thread
	Tracer tracer.Tracer
}

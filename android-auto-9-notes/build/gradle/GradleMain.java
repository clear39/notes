//  @/work/tools/gradle-5.2.1/src/launcher/org/gradle/launcher/GradleMain.java
public class GradleMain {
    public static void main(String[] args) throws Exception {
        new ProcessBootstrap().run("org.gradle.launcher.Main", args);
    }
}


//  @/work/tools/gradle-5.2.1/src/launcher/org/gradle/launcher/bootstrap/ProcessBootstrap.java
public class ProcessBootstrap {
    /**
     * Sets up the ClassLoader structure for the given class, creates an instance and invokes {@link EntryPoint#run(String[])} on it.
     */
    public void run(String mainClassName, String[] args) {
        try {
            runNoExit(mainClassName /* = "org.gradle.launcher.Main" */, args);
            System.exit(0);
        } catch (Throwable throwable) {
            throwable.printStackTrace();
            System.exit(1);
        }
    }

    private void runNoExit(String mainClassName, String[] args) throws Exception {
        ClassPathRegistry classPathRegistry = new DefaultClassPathRegistry(new DefaultClassPathProvider(new DefaultModuleRegistry(CurrentGradleInstallation.get())));
        ClassLoaderFactory classLoaderFactory = new DefaultClassLoaderFactory();
        ClassPath antClasspath = classPathRegistry.getClassPath("ANT");
        ClassPath runtimeClasspath = classPathRegistry.getClassPath("GRADLE_RUNTIME");
        ClassLoader antClassLoader = classLoaderFactory.createIsolatedClassLoader("ant-loader", antClasspath);
        ClassLoader runtimeClassLoader = new VisitableURLClassLoader("ant-and-gradle-loader", antClassLoader, runtimeClasspath);

        ClassLoader oldClassLoader = Thread.currentThread().getContextClassLoader();
        Thread.currentThread().setContextClassLoader(runtimeClassLoader);

        try {
            Class<?> mainClass = runtimeClassLoader.loadClass(mainClassName);
            Object entryPoint = mainClass.getConstructor().newInstance();
            Method mainMethod = mainClass.getMethod("run", String[].class);
            mainMethod.invoke(entryPoint, new Object[]{args});
        } finally {
            Thread.currentThread().setContextClassLoader(oldClassLoader);

            ClassLoaderUtils.tryClose(runtimeClassLoader);
            ClassLoaderUtils.tryClose(antClassLoader);
        }
    }
}


//  @/work/tools/gradle-5.2.1/src/launcher/org/gradle/launcher/bootstrap/EntryPoint.java
public abstract class EntryPoint {

    /**
     * Unless the createCompleter() method is overridden, the JVM will exit before returning from this method.
     */
    public void run(String[] args) {
        RecordingExecutionListener listener = new RecordingExecutionListener();
        try {
            doAction(args, listener);
        } catch (Throwable e) {
            createErrorHandler().execute(e);
            listener.onFailure(e);
        }

        Throwable failure = listener.getFailure();
        ExecutionCompleter completer = createCompleter();
        if (failure == null) {
            completer.complete();
        } else {
            completer.completeWithFailure(failure);
        }
    }



    protected ExecutionCompleter createCompleter() {
        return new ProcessCompleter();
    }



}


//  @/work/tools/gradle-5.2.1/src/launcher/org/gradle/launcher/Main.java
public class Main extends EntryPoint {
    public static void main(String[] args) {
        new Main().run(args);
    }

    protected void doAction(String[] args, ExecutionListener listener) {
        UnsupportedJavaRuntimeException.assertUsingVersion("Gradle", JavaVersion.VERSION_1_8);
        createActionFactory().convert(Arrays.asList(args)).execute(listener);
    }

    CommandLineActionFactory createActionFactory() {
        return new CommandLineActionFactory();
    }
}


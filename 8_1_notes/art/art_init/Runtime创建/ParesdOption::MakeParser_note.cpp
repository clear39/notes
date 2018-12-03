
//	 auto parser = MakeParser(ignore_unrecognized);

std::unique_ptr<RuntimeParser> ParsedOptions::MakeParser(bool ignore_unrecognized = JNI_FALSE) {
  using M = RuntimeArgumentMap;

  //	@art/runtime/parsed_options.h:46:  using RuntimeParser = CmdlineParser<RuntimeArgumentMap, RuntimeArgumentMap::Key>;
  //	@art/cmdline/cmdline_parser.h:43:   CmdlineParser 
  std::unique_ptr<RuntimeParser::Builder> parser_builder = std::unique_ptr<RuntimeParser::Builder>(new RuntimeParser::Builder());
/*
06-25 21:24:56.533  2798  2798 D AndroidRuntime: >>>>>> START com.android.internal.os.ZygoteInit uid 0 <<<<<<
06-25 21:24:56.581  2798  2798 I zygote  : option[0]=-Xzygote
06-25 21:24:56.581  2798  2798 I zygote  : option[1]=-Xusetombstonedtraces
06-25 21:24:56.581  2798  2798 I zygote  : option[2]=exit
06-25 21:24:56.581  2798  2798 I zygote  : option[3]=vfprintf
06-25 21:24:56.581  2798  2798 I zygote  : option[4]=sensitiveThread
06-25 21:24:56.581  2798  2798 I zygote  : option[5]=-verbose:gc
06-25 21:24:56.581  2798  2798 I zygote  : option[6]=-Xms16m
06-25 21:24:56.581  2798  2798 I zygote  : option[7]=-Xmx512m
06-25 21:24:56.581  2798  2798 I zygote  : option[8]=-XX:HeapGrowthLimit=192m
06-25 21:24:56.581  2798  2798 I zygote  : option[9]=-XX:HeapMinFree=512k
06-25 21:24:56.581  2798  2798 I zygote  : option[10]=-XX:HeapMaxFree=8m
06-25 21:24:56.581  2798  2798 I zygote  : option[11]=-XX:HeapTargetUtilization=0.75
06-25 21:24:56.581  2798  2798 I zygote  : option[12]=-Xusejit:true
06-25 21:24:56.581  2798  2798 I zygote  : option[13]=-Xjitsaveprofilinginfo
06-25 21:24:56.581  2798  2798 I zygote  : option[14]=-agentlib:jdwp=transport=dt_android_adb,suspend=n,server=y
06-25 21:24:56.582  2798  2798 I zygote  : option[15]=-Xlockprofthreshold:500
06-25 21:24:56.582  2798  2798 I zygote  : option[16]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[17]=--runtime-arg
06-25 21:24:56.582  2798  2798 I zygote  : option[18]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[19]=-Xms64m
06-25 21:24:56.582  2798  2798 I zygote  : option[20]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[21]=--runtime-arg
06-25 21:24:56.582  2798  2798 I zygote  : option[22]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[23]=-Xmx64m
06-25 21:24:56.582  2798  2798 I zygote  : option[24]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[25]=--image-classes=/system/etc/preloaded-classes
06-25 21:24:56.582  2798  2798 I zygote  : option[26]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[27]=--compiled-classes=/system/etc/compiled-classes
06-25 21:24:56.582  2798  2798 I zygote  : option[28]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[29]=--dirty-image-objects=/system/etc/dirty-image-objects
06-25 21:24:56.582  2798  2798 I zygote  : option[30]=-Xcompiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[31]=--runtime-arg
06-25 21:24:56.582  2798  2798 I zygote  : option[32]=-Xcompiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[33]=-Xms64m
06-25 21:24:56.582  2798  2798 I zygote  : option[34]=-Xcompiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[35]=--runtime-arg
06-25 21:24:56.582  2798  2798 I zygote  : option[36]=-Xcompiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[37]=-Xmx512m
06-25 21:24:56.582  2798  2798 I zygote  : option[38]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[39]=--instruction-set-variant=cortex-a9
06-25 21:24:56.582  2798  2798 I zygote  : option[40]=-Xcompiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[41]=--instruction-set-variant=cortex-a9
06-25 21:24:56.583  2798  2798 I zygote  : option[42]=-Ximage-compiler-option
06-25 21:24:56.583  2798  2798 I zygote  : option[43]=--instruction-set-features=default
06-25 21:24:56.583  2798  2798 I zygote  : option[44]=-Xcompiler-option
06-25 21:24:56.583  2798  2798 I zygote  : option[45]=--instruction-set-features=default
06-25 21:24:56.583  2798  2798 I zygote  : option[46]=-Duser.locale=en-US
06-25 21:24:56.583  2798  2798 I zygote  : option[47]=--cpu-abilist=armeabi-v7a,armeabi
06-25 21:24:56.583  2798  2798 I zygote  : option[48]=-Xfingerprint:Android/autolink_8qxp/autolink_8qxp:8.1.0/1.2.0-beta2-rc3/20180724:userdebug/dev-keys
*/
  parser_builder->
       Define("-Xzygote")
          .IntoKey(M::Zygote)
      .Define("-help")
          .IntoKey(M::Help)
      .Define("-showversion")
          .IntoKey(M::ShowVersion)
      .Define("-Xbootclasspath:_")
          .WithType<std::string>()
          .IntoKey(M::BootClassPath)
      .Define("-Xbootclasspath-locations:_")
          .WithType<ParseStringList<':'>>()  // std::vector<std::string>, split by :
          .IntoKey(M::BootClassPathLocations)
      .Define({"-classpath _", "-cp _"})
          .WithType<std::string>()
          .IntoKey(M::ClassPath)
      .Define("-Ximage:_")
          .WithType<std::string>()
          .IntoKey(M::Image)
      .Define("-Xcheck:jni")
          .IntoKey(M::CheckJni)
      .Define("-Xjniopts:forcecopy")
          .IntoKey(M::JniOptsForceCopy)
      .Define({"-Xrunjdwp:_", "-agentlib:jdwp=_"})
          .WithType<JDWP::JdwpOptions>()
          .IntoKey(M::JdwpOptions)
      // TODO Re-enable -agentlib: once I have a good way to transform the values.
      // .Define("-agentlib:_")
      //     .WithType<std::vector<ti::Agent>>().AppendValues()
      //     .IntoKey(M::AgentLib)
      .Define("-agentpath:_")
          .WithType<std::list<ti::Agent>>().AppendValues()
          .IntoKey(M::AgentPath)
      .Define("-Xms_")
          .WithType<MemoryKiB>()
          .IntoKey(M::MemoryInitialSize)
      .Define("-Xmx_")
          .WithType<MemoryKiB>()
          .IntoKey(M::MemoryMaximumSize)
      .Define("-XX:HeapGrowthLimit=_")
          .WithType<MemoryKiB>()
          .IntoKey(M::HeapGrowthLimit)
      .Define("-XX:HeapMinFree=_")
          .WithType<MemoryKiB>()
          .IntoKey(M::HeapMinFree)
      .Define("-XX:HeapMaxFree=_")
          .WithType<MemoryKiB>()
          .IntoKey(M::HeapMaxFree)
      .Define("-XX:NonMovingSpaceCapacity=_")
          .WithType<MemoryKiB>()
          .IntoKey(M::NonMovingSpaceCapacity)
      .Define("-XX:HeapTargetUtilization=_")
          .WithType<double>().WithRange(0.1, 0.9)
          .IntoKey(M::HeapTargetUtilization)
      .Define("-XX:ForegroundHeapGrowthMultiplier=_")
          .WithType<double>().WithRange(0.1, 5.0)
          .IntoKey(M::ForegroundHeapGrowthMultiplier)
      .Define("-XX:ParallelGCThreads=_")
          .WithType<unsigned int>()
          .IntoKey(M::ParallelGCThreads)
      .Define("-XX:ConcGCThreads=_")
          .WithType<unsigned int>()
          .IntoKey(M::ConcGCThreads)
      .Define("-Xss_")
          .WithType<Memory<1>>()
          .IntoKey(M::StackSize)
      .Define("-XX:MaxSpinsBeforeThinLockInflation=_")
          .WithType<unsigned int>()
          .IntoKey(M::MaxSpinsBeforeThinLockInflation)
      .Define("-XX:LongPauseLogThreshold=_")  // in ms
          .WithType<MillisecondsToNanoseconds>()  // store as ns
          .IntoKey(M::LongPauseLogThreshold)
      .Define("-XX:LongGCLogThreshold=_")  // in ms
          .WithType<MillisecondsToNanoseconds>()  // store as ns
          .IntoKey(M::LongGCLogThreshold)
      .Define("-XX:DumpGCPerformanceOnShutdown")
          .IntoKey(M::DumpGCPerformanceOnShutdown)
      .Define("-XX:DumpJITInfoOnShutdown")
          .IntoKey(M::DumpJITInfoOnShutdown)
      .Define("-XX:IgnoreMaxFootprint")
          .IntoKey(M::IgnoreMaxFootprint)
      .Define("-XX:LowMemoryMode")
          .IntoKey(M::LowMemoryMode)
      .Define("-XX:UseTLAB")
          .WithValue(true)
          .IntoKey(M::UseTLAB)
      .Define({"-XX:EnableHSpaceCompactForOOM", "-XX:DisableHSpaceCompactForOOM"})
          .WithValues({true, false})
          .IntoKey(M::EnableHSpaceCompactForOOM)
      .Define("-XX:DumpNativeStackOnSigQuit:_")
          .WithType<bool>()
          .WithValueMap({{"false", false}, {"true", true}})
          .IntoKey(M::DumpNativeStackOnSigQuit)
      .Define("-XX:MadviseRandomAccess:_")
          .WithType<bool>()
          .WithValueMap({{"false", false}, {"true", true}})
          .IntoKey(M::MadviseRandomAccess)
      .Define("-Xusejit:_")
          .WithType<bool>()
          .WithValueMap({{"false", false}, {"true", true}})
          .IntoKey(M::UseJitCompilation)
      .Define("-Xjitinitialsize:_")
          .WithType<MemoryKiB>()
          .IntoKey(M::JITCodeCacheInitialCapacity)
      .Define("-Xjitmaxsize:_")
          .WithType<MemoryKiB>()
          .IntoKey(M::JITCodeCacheMaxCapacity)
      .Define("-Xjitthreshold:_")
          .WithType<unsigned int>()
          .IntoKey(M::JITCompileThreshold)
      .Define("-Xjitwarmupthreshold:_")
          .WithType<unsigned int>()
          .IntoKey(M::JITWarmupThreshold)
      .Define("-Xjitosrthreshold:_")
          .WithType<unsigned int>()
          .IntoKey(M::JITOsrThreshold)
      .Define("-Xjitprithreadweight:_")
          .WithType<unsigned int>()
          .IntoKey(M::JITPriorityThreadWeight)
      .Define("-Xjittransitionweight:_")
          .WithType<unsigned int>()
          .IntoKey(M::JITInvokeTransitionWeight)
      .Define("-Xjitsaveprofilinginfo")
          .WithType<ProfileSaverOptions>()
          .AppendValues()
          .IntoKey(M::ProfileSaverOpts)
      .Define("-Xps-_")  // profile saver options -Xps-<key>:<value>
          .WithType<ProfileSaverOptions>()
          .AppendValues()
          .IntoKey(M::ProfileSaverOpts)  // NOTE: Appends into same key as -Xjitsaveprofilinginfo
      .Define("-XX:HspaceCompactForOOMMinIntervalMs=_")  // in ms
          .WithType<MillisecondsToNanoseconds>()  // store as ns
          .IntoKey(M::HSpaceCompactForOOMMinIntervalsMs)
      .Define("-D_")
          .WithType<std::vector<std::string>>().AppendValues()
          .IntoKey(M::PropertiesList)
      .Define("-Xjnitrace:_")
          .WithType<std::string>()
          .IntoKey(M::JniTrace)
      .Define("-Xpatchoat:_")
          .WithType<std::string>()
          .IntoKey(M::PatchOat)
      .Define({"-Xrelocate", "-Xnorelocate"})
          .WithValues({true, false})
          .IntoKey(M::Relocate)
      .Define({"-Xdex2oat", "-Xnodex2oat"})
          .WithValues({true, false})
          .IntoKey(M::Dex2Oat)
      .Define({"-Ximage-dex2oat", "-Xnoimage-dex2oat"})
          .WithValues({true, false})
          .IntoKey(M::ImageDex2Oat)
      .Define("-Xint")
          .WithValue(true)
          .IntoKey(M::Interpret)
      .Define("-Xgc:_")
          .WithType<XGcOption>()
          .IntoKey(M::GcOption)
      .Define("-XX:LargeObjectSpace=_")
          .WithType<gc::space::LargeObjectSpaceType>()
          .WithValueMap({{"disabled", gc::space::LargeObjectSpaceType::kDisabled},
                         {"freelist", gc::space::LargeObjectSpaceType::kFreeList},
                         {"map",      gc::space::LargeObjectSpaceType::kMap}})
          .IntoKey(M::LargeObjectSpace)
      .Define("-XX:LargeObjectThreshold=_")
          .WithType<Memory<1>>()
          .IntoKey(M::LargeObjectThreshold)
      .Define("-XX:BackgroundGC=_")
          .WithType<BackgroundGcOption>()
          .IntoKey(M::BackgroundGc)
      .Define("-XX:+DisableExplicitGC")
          .IntoKey(M::DisableExplicitGC)
      .Define("-verbose:_")
          .WithType<LogVerbosity>()
          .IntoKey(M::Verbose)
      .Define("-Xlockprofthreshold:_")
          .WithType<unsigned int>()
          .IntoKey(M::LockProfThreshold)
      .Define("-Xstackdumplockprofthreshold:_")
          .WithType<unsigned int>()
          .IntoKey(M::StackDumpLockProfThreshold)
      .Define("-Xusetombstonedtraces")
          .WithValue(true)
          .IntoKey(M::UseTombstonedTraces)
      .Define("-Xstacktracefile:_")
          .WithType<std::string>()
          .IntoKey(M::StackTraceFile)
      .Define("-Xmethod-trace")
          .IntoKey(M::MethodTrace)
      .Define("-Xmethod-trace-file:_")
          .WithType<std::string>()
          .IntoKey(M::MethodTraceFile)
      .Define("-Xmethod-trace-file-size:_")
          .WithType<unsigned int>()
          .IntoKey(M::MethodTraceFileSize)
      .Define("-Xmethod-trace-stream")
          .IntoKey(M::MethodTraceStreaming)
      .Define("-Xprofile:_")
          .WithType<TraceClockSource>()
          .WithValueMap({{"threadcpuclock", TraceClockSource::kThreadCpu},
                         {"wallclock",      TraceClockSource::kWall},
                         {"dualclock",      TraceClockSource::kDual}})
          .IntoKey(M::ProfileClock)
      .Define("-Xcompiler:_")
          .WithType<std::string>()
          .IntoKey(M::Compiler)
      .Define("-Xcompiler-option _")
          .WithType<std::vector<std::string>>()
          .AppendValues()
          .IntoKey(M::CompilerOptions)
      .Define("-Ximage-compiler-option _")
          .WithType<std::vector<std::string>>()
          .AppendValues()
          .IntoKey(M::ImageCompilerOptions)
      .Define("-Xverify:_")
          .WithType<verifier::VerifyMode>()
          .WithValueMap({{"none",     verifier::VerifyMode::kNone},
                         {"remote",   verifier::VerifyMode::kEnable},
                         {"all",      verifier::VerifyMode::kEnable},
                         {"softfail", verifier::VerifyMode::kSoftFail}})
          .IntoKey(M::Verify)
      .Define("-XX:NativeBridge=_")
          .WithType<std::string>()
          .IntoKey(M::NativeBridge)
      .Define("-Xzygote-max-boot-retry=_")
          .WithType<unsigned int>()
          .IntoKey(M::ZygoteMaxFailedBoots)
      .Define("-Xno-dex-file-fallback")
          .IntoKey(M::NoDexFileFallback)
      .Define("-Xno-sig-chain")
          .IntoKey(M::NoSigChain)
      .Define("--cpu-abilist=_")
          .WithType<std::string>()
          .IntoKey(M::CpuAbiList)
      .Define("-Xfingerprint:_")
          .WithType<std::string>()
          .IntoKey(M::Fingerprint)
      .Define("-Xexperimental:_")
          .WithType<ExperimentalFlags>()
          .AppendValues()
          .IntoKey(M::Experimental)
      .Define("-Xforce-nb-testing")
          .IntoKey(M::ForceNativeBridge)
      .Define("-Xplugin:_")
          .WithType<std::vector<Plugin>>()
          .AppendValues()
          .IntoKey(M::Plugins)
      .Define("-XX:ThreadSuspendTimeout=_")  // in ms
          .WithType<MillisecondsToNanoseconds>()  // store as ns
          .IntoKey(M::ThreadSuspendTimeout)
      .Define("-XX:SlowDebug=_")
          .WithType<bool>()
          .WithValueMap({{"false", false}, {"true", true}})
          .IntoKey(M::SlowDebug)
      .Ignore({
          "-ea", "-da", "-enableassertions", "-disableassertions", "--runtime-arg", "-esa",
          "-dsa", "-enablesystemassertions", "-disablesystemassertions", "-Xrs", "-Xint:_",
          "-Xdexopt:_", "-Xnoquithandler", "-Xjnigreflimit:_", "-Xgenregmap", "-Xnogenregmap",
          "-Xverifyopt:_", "-Xcheckdexsum", "-Xincludeselectedop", "-Xjitop:_",
          "-Xincludeselectedmethod", "-Xjitthreshold:_",
          "-Xjitblocking", "-Xjitmethod:_", "-Xjitclass:_", "-Xjitoffset:_",
          "-Xjitconfig:_", "-Xjitcheckcg", "-Xjitverbose", "-Xjitprofile",
          "-Xjitdisableopt", "-Xjitsuspendpoll", "-XX:mainThreadStackSize=_"})
      .IgnoreUnrecognized(ignore_unrecognized);

  // TODO: Move Usage information into this DSL.

  return std::unique_ptr<RuntimeParser>(new RuntimeParser(parser_builder->Build()));
}


//	  parser_builder->Define("-Xzygote").IntoKey(M::Zygote)
/***
1)	Define 的作用 
      得到一个 UntypedArgumentBuilder 实例 ，保存参数字符串"-Xzygote"到其names_中，且内部保存了Builder类对象实例；
  。。。。。。

      通过UntypedArgumentBuilder调用WithType 构造一个对应类型ArgumentBuilder

      其中 UntypedArgumentBuilder 调用 AppendValues 方法   argument_info_.appending_values_ = true;

2)	
IntoKey的作用：
	由Define的到了UntypedArgumentBuilder，再通过IntoKey实现：
	第一 新建一个ArgumentBuilder<Unit>并且传入Builder参数，以及Builder.save_destination_
	第二 将names_ 通过SetNames方法再次传入给ArgumentBuilder<Unit>的成员argument_info_.names_
	第三 调用ArgumentBuilder<Unit>的IntoKey方法，再次传入M::Zygote
	第四 在ArgumentBuilder<Unit>的IntoKey方法里，给save_value_和load_value_这俩个方法（注意save_value_和load_value_俩个方法操作的就是 save_destination_中的SaveToMap和GetOrCreateFromMap）
	第五 调用 ArgumentBuilder<Unit>的CompleteArgument方法
		 调用 argument_info_.CompleteArgument() 方法，对"-Xzygote"处理
		 调用ArgumentBuilder<Unit>.AppendCompletedArgument()方法，创建一个CmdlineParseArgument对象传入 argument_info_ ， save_value_ 和 load_value_，
		 Builder通过AppendCompletedArgument方法将CmdlineParseArgument实例加入到completed_arguments_成员中
*/


// Build a new parser given a chain of calls to define arguments.
struct Builder {
    Builder() : save_destination_(new SaveDestination()) {}

    // Define a single argument. The default type is Unit.
	UntypedArgumentBuilder Builder::Define(const char* name) {
	  return Define({name});
	}

	// Define a single argument with multiple aliases.
	UntypedArgumentBuilder Builder::Define(std::initializer_list<const char*> names) {
	  auto&& b = UntypedArgumentBuilder(*this);
	  b.SetNames(names);
	  return std::move(b);
	}




}



struct UntypedArgumentBuilder {
	explicit UntypedArgumentBuilder(CmdlineParser::Builder& parent) : parent_(parent) {}//	CmdlineParser::Builder& parent_;
	
	void SetNames(std::vector<const char*>&& names) {
      names_ = std::move(names);//	std::vector<const char*> names_;
    }


    // Set the current building argument to target this key.
    // When this command line argument is parsed, it can be fetched with this key.
    Builder& IntoKey(const TVariantMapKey<Unit>& key) {
      return CreateTypedBuilder<Unit>().IntoKey(key);
    }

    template <typename TArg>
    ArgumentBuilder<TArg> CreateTypedBuilder() {
      auto&& b = CreateArgumentBuilder<TArg>(parent_);
      InitializeTypedBuilder(&b);  // Type-specific initialization
      b.SetNames(std::move(names_));
      return std::move(b);
    }


    template <typename TArg>
    ArgumentBuilder<TArg> WithType() {
      return CreateTypedBuilder<TArg>();
    }

}


// This has to be defined after everything else, since we want the builders to call this.
template <typename TVariantMap,template <typename TKeyValue> class TVariantMapKey >
template <typename TArg>
typename CmdlineParser<TVariantMap, TVariantMapKey>::template ArgumentBuilder<TArg>
CmdlineParser<TVariantMap, TVariantMapKey>::CreateArgumentBuilder(CmdlineParser<TVariantMap, TVariantMapKey>::Builder& parent) {
  return CmdlineParser<TVariantMap, TVariantMapKey>::ArgumentBuilder<TArg>(parent, parent.save_destination_);
}


template <typename TArg>
struct ArgumentBuilder {

  CmdlineParserArgumentInfo<TArg> argument_info_;// @art/cmdline/detail/cmdline_parse_argument_detail.h:

    ArgumentBuilder(CmdlineParser::Builder& parser,std::shared_ptr<SaveDestination> save_destination)
        : parent_(parser),
          save_value_specified_(false),
          load_value_specified_(false),
          save_destination_(save_destination) {

      save_value_ = [](TArg&) {
        assert(false && "No save value function defined");
      };

      load_value_ = []() -> TArg& {
        assert(false && "No load value function defined");
        return *reinterpret_cast<TArg*>(0);  // Blow up.
      };
    }

    // Map the parsed string values (from _) onto a concrete value. If no wildcard
    // has been specified, then map the value directly from the arg name (i.e.
    // if there are multiple aliases, then use the alias to do the mapping).
    //
    // Do not use if a values list has already been set.
    ArgumentBuilder<TArg>& WithValueMap(std::initializer_list<std::pair<const char*, TArg>> key_value_list) {
      assert(!argument_info_.has_value_list_);
      argument_info_.has_value_map_ = true;
      argument_info_.value_map_ = key_value_list;
      return *this;
    }


        // If this argument is seen multiple times, successive arguments mutate the same value
    // instead of replacing it with a new value.
    ArgumentBuilder<TArg>& AppendValues() {
      argument_info_.appending_values_ = true;
      return *this;
    }




    void SetNames(std::initializer_list<const char*> names) {
      argument_info_.names_ = names;	//	detail::CmdlineParserArgumentInfo<TArg> argument_info_;
    }

      // Write the results of this argument into the key.
    // To look up the parsed arguments, get the map and then use this key with VariantMap::Get
    CmdlineParser::Builder& IntoKey(const MapKey& key) {
      // Only capture save destination as a pointer.
      // This allows the parser to later on change the specific save targets.
      auto save_destination = save_destination_;
      save_value_ = [save_destination, &key](TArg& value) {
        save_destination->SaveToMap(key, value);
        CMDLINE_DEBUG_LOG << "Saved value into map '"  << detail::ToStringAny(value) << "'" << std::endl;
      };

      load_value_ = [save_destination, &key]() -> TArg& {
        TArg& value = save_destination->GetOrCreateFromMap(key);
        CMDLINE_DEBUG_LOG << "Loaded value from map '" << detail::ToStringAny(value) << "'"  << std::endl;
        return value;
      };

      save_value_specified_ = true;
      load_value_specified_ = true;

      CompleteArgument();
      return parent_;
    }

     // Called by any function that doesn't chain back into this builder.
    // Completes the argument builder and save the information into the main builder.
    void CompleteArgument() {
      assert(save_value_specified_ && "No Into... function called, nowhere to save parsed values to");
      assert(load_value_specified_ && "No Into... function called, nowhere to load parsed values from");

      argument_info_.CompleteArgument();//	argument_info_@art/cmdline/detail/cmdline_parse_argument_detail.h:90

      // Appending the completed argument is destructive. The object is no longer
      // usable since all the useful information got moved out of it.
      AppendCompletedArgument(parent_,new detail::CmdlineParseArgument<TArg>(std::move(argument_info_),std::move(save_value_),std::move(load_value_)));
    }

}

// This has to be defined after everything else, since we want the builders to call this.
template <typename TVariantMap,template <typename TKeyValue> class TVariantMapKey>
void CmdlineParser<TVariantMap, TVariantMapKey>::AppendCompletedArgument(CmdlineParser<TVariantMap, TVariantMapKey>::Builder& builder,detail::CmdlineParseArgumentAny* arg) {
  builder.AppendCompletedArgument(arg);
}





// Build a new parser given a chain of calls to define arguments.
struct Builder {

	void AppendCompletedArgument(detail::CmdlineParseArgumentAny* arg) {
      auto smart_ptr = std::unique_ptr<detail::CmdlineParseArgumentAny>(arg);
      completed_arguments_.push_back(std::move(smart_ptr));//std::vector<std::unique_ptr<detail::CmdlineParseArgumentAny>> completed_arguments_;
    }


     // Whether the parser should give up on unrecognized arguments. Not recommended.
    Builder& IgnoreUnrecognized(bool ignore_unrecognized) {
      ignore_unrecognized_ = ignore_unrecognized;
      return *this;
    }




	// Finish building the parser; performs sanity checks. Return value is moved, not copied.
    // Do not call this more than once.
    CmdlineParser Build() {
      assert(!built_);
      built_ = true;

      auto&& p = CmdlineParser(ignore_unrecognized_, std::move(ignore_list_), save_destination_, std::move(completed_arguments_));

      return std::move(p);
    }

    
}



struct CmdlineParser{
  // Construct a new parser from the builder. Move all the arguments.
  CmdlineParser(bool ignore_unrecognized,
                std::vector<const char*>&& ignore_list,
                std::shared_ptr<SaveDestination> save_destination,
                std::vector<std::unique_ptr<detail::CmdlineParseArgumentAny>>&& completed_arguments)
    : ignore_unrecognized_(ignore_unrecognized),
      ignore_list_(std::move(ignore_list)),
      save_destination_(save_destination),
      completed_arguments_(std::move(completed_arguments)) {
    assert(save_destination != nullptr);
  }
}



template <typename TArg>
struct CmdlineParserArgumentInfo {
    // Mark the argument definition as completed, do not mutate the object anymore after this
      // call is done.
      //
      // Performs several sanity checks and token calculations.
      void CompleteArgument() {
        assert(names_.size() >= 1);
        assert(!is_completed_);

        is_completed_ = true;

        size_t blank_count = 0;
        size_t token_count = 0;

        size_t global_blank_count = 0;
        size_t global_token_count = 0;
        for (auto&& name : names_) {
          std::string s(name);

          size_t local_blank_count = std::count(s.begin(), s.end(), '_');
          size_t local_token_count = std::count(s.begin(), s.end(), ' ');

          if (global_blank_count != 0) {
            assert(local_blank_count == global_blank_count
                   && "Every argument descriptor string must have same amount of blanks (_)");
          }

          if (local_blank_count != 0) {
            global_blank_count = local_blank_count;
            blank_count++;

            assert(local_blank_count == 1 && "More than one blank is not supported");
            assert(s.back() == '_' && "The blank character must only be at the end of the string");
          }

          if (global_token_count != 0) {
            assert(local_token_count == global_token_count
                   && "Every argument descriptor string must have same amount of tokens (spaces)");
          }

          if (local_token_count != 0) {
            global_token_count = local_token_count;
            token_count++;
          }

          // Tokenize every name, turning it from a string to a token list.
          tokenized_names_.clear();
          for (auto&& name1 : names_) {
            // Split along ' ' only, removing any duplicated spaces.
            tokenized_names_.push_back(
                TokenRange::Split(name1, {' '}).RemoveToken(" "));
          }

          // remove the _ character from each of the token ranges
          // we will often end up with an empty token (i.e. ["-XX", "_"] -> ["-XX", ""]
          // and this is OK because we still need an empty token to simplify
          // range comparisons
          simple_names_.clear();

          for (auto&& tokenized_name : tokenized_names_) {
            simple_names_.push_back(tokenized_name.RemoveCharacter('_'));
          }
        }

        if (token_count != 0) {
          assert(("Every argument descriptor string must have equal amount of tokens (spaces)" && token_count == names_.size()));
        }

        if (blank_count != 0) {
          assert(("Every argument descriptor string must have an equal amount of blanks (_)" && blank_count == names_.size()));
        }

        using_blanks_ = blank_count > 0;
        {
          size_t smallest_name_token_range_size =  std::accumulate(tokenized_names_.begin(), tokenized_names_.end(), ~(0u),[](size_t min, const TokenRange& cur) {
                                                                                                                                                                                                              return std::min(min, cur.Size());
                                                                                                                                                                                                            });
          size_t largest_name_token_range_size = std::accumulate(tokenized_names_.begin(), tokenized_names_.end(), 0u,[](size_t max, const TokenRange& cur) {
                                                                                                                                                                                                          return std::max(max, cur.Size());
                                                                                                                                                                                                        });

          token_range_size_ = std::make_pair(smallest_name_token_range_size,largest_name_token_range_size);
        }

        if (has_value_list_) {
          assert(names_.size() == value_list_.size()  && "Number of arg descriptors must match number of values");
          assert(!has_value_map_);
        }
        if (has_value_map_) {
          if (!using_blanks_) {
            assert(names_.size() == value_map_.size() && "Since no blanks were specified, each arg is mapped directly into a mapped " "value without parsing; sizes must match");
          }

          assert(!has_value_list_);
        }

        if (!using_blanks_ && !CmdlineType<TArg>::kCanParseBlankless) {
          assert((has_value_map_ || has_value_list_) && "Arguments without a blank (_) must provide either a value map or a value list");
        }

        TypedCheck();//这里空实现
      }
}
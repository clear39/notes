

//	CmdlineParser<RuntimeArgumentMap, RuntimeArgumentMap::Key>;

template <typename TVariantMap,template <typename TKeyValue> class TVariantMapKey>
struct CmdlineParser {

	// Build a new parser given a chain of calls to define arguments.
	struct Builder {
	   	Builder() : save_destination_(new SaveDestination()) {}

	   	// Define a single argument. The default type is Unit.
	    UntypedArgumentBuilder Define(const char* name = "-Xzygote") {
	      return Define({name});
	    }

	    // Define a single argument with multiple aliases.
	    UntypedArgumentBuilder Define(std::initializer_list<const char*> names) {
	      auto&& b = UntypedArgumentBuilder(*this);
	      b.SetNames(names);
	      return std::move(b);
	    }
	}


	struct UntypedArgumentBuilder {
    	// Set a type for this argument. The specific subcommand parser is looked up by the type.
    	explicit UntypedArgumentBuilder(CmdlineParser::Builder& parent) : parent_(parent) {}
    	// UntypedArgumentBuilder(UntypedArgumentBuilder&& other) = default;
    	CmdlineParser::Builder& parent_;

    	std::vector<const char*> names_;
    	void SetNames(std::initializer_list<const char*> names) {
	      names_ = names;
	    }


	    // Set the current building argument to target this key.
	    // When this command line argument is parsed, it can be fetched with this key.
	    Builder& IntoKey(const TVariantMapKey<Unit>& key = M::Zygote) {
	      return CreateTypedBuilder<Unit>().IntoKey(key);
	    }

	    template <typename TArg> // TArg = Unit
	    ArgumentBuilder<TArg> CreateTypedBuilder() {
	      auto&& b = CreateArgumentBuilder<TArg>(parent_);
	      InitializeTypedBuilder(&b);  // Type-specific initialization
	      b.SetNames(std::move(names_));
	      return std::move(b);
	    }

	    template <typename TArg = Unit>	// TArg = Unit
	    typename std::enable_if<std::is_same<TArg, Unit>::value>::type
	    InitializeTypedBuilder(ArgumentBuilder<TArg>* arg_builder) {
	      // Every Unit argument implicitly maps to a runtime value of Unit{}
	      std::vector<Unit> values(names_.size(), Unit{});  // NOLINT [whitespace/braces] [5]
	      arg_builder->SetValuesInternal(std::move(values));
	    }


	}



		// This has to be defined after everything else, since we want the builders to call this.
	template <typename TVariantMap,template <typename TKeyValue> class TVariantMapKey>
	template <typename TArg>
	typename CmdlineParser<TVariantMap, TVariantMapKey>::template ArgumentBuilder<TArg>
	CmdlineParser<TVariantMap, TVariantMapKey>::CreateArgumentBuilder(CmdlineParser<TVariantMap, TVariantMapKey>::Builder& parent) {
	  return CmdlineParser<TVariantMap, TVariantMapKey>::ArgumentBuilder<TArg>(parent, parent.save_destination_);
	}


	// Builder for the argument definition of type TArg. Do not use this type directly,
	// it is only a separate type to provide compile-time enforcement against doing
	// illegal builds.
	template <typename TArg>
	struct ArgumentBuilder {

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

		void SetValuesInternal(const std::vector<TArg>&& value_list) {
	      assert(!argument_info_.has_value_map_);

	      argument_info_.has_value_list_ = true;
	      argument_info_.value_list_ = value_list;
	    }

	    detail::CmdlineParserArgumentInfo<TArg> argument_info_;

	}


  	struct SaveDestination {
    	SaveDestination() : variant_map_(new TVariantMap()) {}
	}


}
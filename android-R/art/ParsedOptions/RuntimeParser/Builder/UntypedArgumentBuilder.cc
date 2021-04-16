
//  @   art/cmdline/cmdline_parser.h



struct UntypedArgumentBuilder {
    // Set a type for this argument. The specific subcommand parser is looked up by the type.
    template <typename TArg>
    ArgumentBuilder<TArg> WithType() {
      return CreateTypedBuilder<TArg>();
    }

    // When used with multiple aliases, map the position of the alias to the value position.
    template <typename TArg>
    ArgumentBuilder<TArg> WithValues(std::initializer_list<TArg> values) {
      auto&& a = CreateTypedBuilder<TArg>();
      a.WithValues(values);
      return std::move(a);
    }

    // When used with a single alias, map the alias into this value.
    // Same as 'WithValues({value})' , but allows the omission of the curly braces {}.
    template <typename TArg>
    ArgumentBuilder<TArg> WithValue(const TArg& value) {
      return WithValues({ value });
    }

    // Set the current building argument to target this key.
    // When this command line argument is parsed, it can be fetched with this key.
    /**
     * 
    */
    Builder& IntoKey(const TVariantMapKey<Unit>& key) {
      return CreateTypedBuilder<Unit>().IntoKey(key);
    }

    // Ensure we always move this when returning a new builder.
    UntypedArgumentBuilder(UntypedArgumentBuilder&&) = default;

   protected:
    void SetNames(std::vector<const char*>&& names) {
      names_ = std::move(names);
    }

    /**
     * 第二步 UntypedArgumentBuilder在Builder.define中构建的
    */
    void SetNames(std::initializer_list<const char*> names) {
      names_ = names;
    }

   private:
    // No copying. Move instead.
    UntypedArgumentBuilder(const UntypedArgumentBuilder&) = delete;

    template <typename TArg>
    ArgumentBuilder<TArg> CreateTypedBuilder() {
        /**
         * 这里为CmdlineParser<TVariantMap, TVariantMapKey>中的CreateArgumentBuilder
         * 返回值为一个 ArgumentBuilder<TArg> 变量
        */
      auto&& b = CreateArgumentBuilder<TArg>(parent_);
      /**
       * 
      */
      InitializeTypedBuilder(&b);  // Type-specific initialization
      b.SetNames(std::move(names_));
      return std::move(b);
    }

    template <typename TArg = Unit>
    typename std::enable_if<std::is_same<TArg, Unit>::value>::type
    InitializeTypedBuilder(ArgumentBuilder<TArg>* arg_builder) {
      // Every Unit argument implicitly maps to a runtime value of Unit{}
      std::vector<Unit> values(names_.size(), Unit{});
      arg_builder->SetValuesInternal(std::move(values));
    }

    // No extra work for all other types
    void InitializeTypedBuilder(void*) {}

    template <typename TArg>
    friend struct ArgumentBuilder;
    friend struct Builder;


    /**
     * 第一步 UntypedArgumentBuilder在Builder.define中构建的
    */
    explicit UntypedArgumentBuilder(CmdlineParser::Builder& parent) : parent_(parent) {}
    // UntypedArgumentBuilder(UntypedArgumentBuilder&& other) = default;

    CmdlineParser::Builder& parent_;
    std::vector<const char*> names_;
};




//   return CreateTypedBuilder<Unit>().IntoKey(key);
template <typename Unit>
ArgumentBuilder<Unit> CreateTypedBuilder() {
    auto&& b = CreateArgumentBuilder<Unit>(parent_);
    InitializeTypedBuilder(&b);  // Type-specific initialization
    b.SetNames(std::move(names_));
    return std::move(b);
}




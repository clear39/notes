//  @   art/cmdline/cmdline_parser.h


struct Builder {
    Builder() : save_destination_(new SaveDestination()) {}

    // Define a single argument. The default type is Unit.
    UntypedArgumentBuilder Define(const char* name) {
      return Define({name});
    }

    // Define a single argument with multiple aliases.
    UntypedArgumentBuilder Define(std::initializer_list<const char*> names) {
      /**
       * 构建 UntypedArgumentBuilder 并将 Builder 传递进去
       * 
      */
      auto&& b = UntypedArgumentBuilder(*this);
      /**
       * 存储到 UntypedArgumentBuilder 中的names成员中；
      */
      b.SetNames(names);
      return std::move(b);
    }

    // Whether the parser should give up on unrecognized arguments. Not recommended.
    Builder& IgnoreUnrecognized(bool ignore_unrecognized) {
      ignore_unrecognized_ = ignore_unrecognized;
      return *this;
    }

    // Provide a list of arguments to ignore for backwards compatibility.
    Builder& Ignore(std::initializer_list<const char*> ignore_list) {
      for (auto&& ignore_name : ignore_list) {
        std::string ign = ignore_name;

        // Ignored arguments are just like a regular definition which have very
        // liberal parsing requirements (no range checks, no value checks).
        // Unlike regular argument definitions, when a value gets parsed into its
        // stronger type, we just throw it away.

        if (ign.find('_') != std::string::npos) {  // Does the arg-def have a wildcard?
          // pretend this is a string, e.g. -Xjitconfig:<anythinggoeshere>
          auto&& builder = Define(ignore_name).template WithType<std::string>().IntoIgnore();
          assert(&builder == this);
          (void)builder;  // Ignore pointless unused warning, it's used in the assert.
        } else {
          // pretend this is a unit, e.g. -Xjitblocking
          auto&& builder = Define(ignore_name).template WithType<Unit>().IntoIgnore();
          assert(&builder == this);
          (void)builder;  // Ignore pointless unused warning, it's used in the assert.
        }
      }
      ignore_list_ = ignore_list;
      return *this;
    }

    // Finish building the parser; performs sanity checks. Return value is moved, not copied.
    // Do not call this more than once.
    CmdlineParser Build() {
      assert(!built_);
      built_ = true;

      auto&& p = CmdlineParser(ignore_unrecognized_,
                               std::move(ignore_list_),
                               save_destination_,
                               std::move(completed_arguments_));

      return std::move(p);
    }

   protected:
    void AppendCompletedArgument(detail::CmdlineParseArgumentAny* arg) {
      auto smart_ptr = std::unique_ptr<detail::CmdlineParseArgumentAny>(arg);
      completed_arguments_.push_back(std::move(smart_ptr));
    }

   private:
    // No copying now!
    Builder(const Builder& other) = delete;

    template <typename TArg>
    friend struct ArgumentBuilder;
    friend struct UntypedArgumentBuilder;
    friend struct CmdlineParser;

    bool built_ = false;
    bool ignore_unrecognized_ = false;
    std::vector<const char*> ignore_list_;
    std::shared_ptr<SaveDestination> save_destination_;

    std::vector<std::unique_ptr<detail::CmdlineParseArgumentAny>> completed_arguments_;
};

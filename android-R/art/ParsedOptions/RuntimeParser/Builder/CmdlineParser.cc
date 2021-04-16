
//  @   art/cmdline/cmdline_parser.h
template <typename TVariantMap,
          template <typename TKeyValue> class TVariantMapKey>
struct CmdlineParser {

}




//  auto&& b = CreateArgumentBuilder<Unit>(parent_);
template <typename TVariantMap,  template <typename TKeyValue> class TVariantMapKey>
template <typename TArg>
typename CmdlineParser<TVariantMap, TVariantMapKey>::template ArgumentBuilder<TArg>  //typename CmdlineParser<TVariantMap, TVariantMapKey>::template ArgumentBuilder<Unit>
CmdlineParser<TVariantMap, TVariantMapKey>::CreateArgumentBuilder(
    CmdlineParser<TVariantMap, TVariantMapKey>::Builder& parent) {
  return CmdlineParser<TVariantMap, TVariantMapKey>::ArgumentBuilder<Unit>(
      parent, parent.save_destination_);
}
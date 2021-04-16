
// 静态方法
std::unique_ptr<const LoadedArsc> LoadedArsc::CreateEmpty() {
  return std::unique_ptr<LoadedArsc>(new LoadedArsc());
}

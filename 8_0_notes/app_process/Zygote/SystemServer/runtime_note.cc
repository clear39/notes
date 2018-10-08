//      art/runtime/runtime.cc

bool Runtime::IsZygote() const {
  return is_zygote_;  //这里是在Runtime的init函数中赋值
}


void Runtime::PreZygoteFork() {
  heap_->PreZygoteFork();
}
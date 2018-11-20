//@hardware/interfaces/boot/1.0/default/service.cpp
int main (int /* argc */, char * /* argv */ []) {
    return defaultPassthroughServiceImplementation<IBootControl>();
}

#define TEST_DEFINE 100

int global_int_shared = 0xCDDCCDDC;


int add(int a, int b) {
    return a+b;
}

int getDefTest() {
    return TEST_DEFINE;
}





int shared = 0xDCCDDCCD;

void swap(int* a, int* b) {
    *a ^= *b ^= *a ^= *b;
}
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "test_src.h"
#include "test_src_b.h"

int A() {
    A1();
    A3();
    if (A4() > 100) {
        A4();
        A5();
        B();
    } else {
        A6();
    }
    B();
    if (!A7()) {
        return A9();
    }
    A8();
    A9();
    return A9() + A10() + A11() + A12();
}

int A1() {
    return 42;
}
int A3() {
    return 42;
}
int A4() {
    return 42;
}
int A5() {
    return 42;
}
int A6() {
    return 42;
}
int A7() {
    return 42;
}
int A8() {
    return 42;
}
int A9() {
    return 42;
}
int A10() {
    return 42;
}
int A11() {
    return 42;
}
int A12() {
    return 42;
}

void B1() {}
void B3() {}
void B4() {}
void B5() {}
void B6() {}
void B7() {}
void B8() {}

int C() {
    if (C1() > 41 && (C2() == 0 || C3())) {
        C3();
        C4();
    }
    C6();
    C7();
    C8();
    D();
    return C9() - C10();
}
int C1() {
    return 4242;
}
int C2() {
    return 4242;
}
int C3() {
    return 4242;
}
int C4() {
    return 4242;
}
int C5() {
    return 4242;
}
int C6() {
    return 4242;
}
int C7() {
    return 4242;
}
int C8() {
    return 4242;
}
int C9() {
    return 4242;
}
int C10() {
    return 4242;
}

void D() {
    D1();
    D3();
    D4();
    D5();
    D6();
}

void D1() {}
void D3() {}
void D5() {}
void D6() {}

void D4() {
    int random = rand() % 10000;

    float res = 0;
    for (int count = 0; count < random; ++count) {
        res += std::sqrt((float) count);
    }
}

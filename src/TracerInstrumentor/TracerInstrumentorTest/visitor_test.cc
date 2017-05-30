#include "visitor_test.h"

int A() {
    return 0;
}

bool B(int b) {
    return b > 500;
}

bool C() {
    return true;
}

bool D() {
    return false;
}

int E(int a, bool b) {
    if (b) {
        return 42;
    }
    return 4242;
}

void F::G() {}

void F::H() {
    G();
    if (B(A())) {
        G();
    }
}

int main() {
    int a = E(A(), C() && D());
    if (B(a))
        return 0;
    else
        return 1;
}

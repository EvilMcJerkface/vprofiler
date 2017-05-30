#include "instrumentor_test.h"

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

int F::H(int a) {
    G();
    if (B(A())) {
        G();
        return A();
    } else if (B(a))
        return D();
    return E(10, C());
}

int main() {
    int a = E(A(), C() && D());
    if (B(a))
        return 0;
    else
        return 1;
}

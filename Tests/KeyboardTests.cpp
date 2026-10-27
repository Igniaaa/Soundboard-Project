#include "KeyboardFilter.h"
#include <cstdlib>
#include <iostream>
using namespace dualdeck;
int checks = 0;
void require(bool condition, const char* name) { if (!condition) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); } ++checks; }
int main()
{
    KeyboardFilter input;
    input.enable(true);
    require(!input.isEnabled(), "cannot arm without device");
    input.select(2); input.enable(true);
    require(input.input(1, 30, true) == KeyboardFilter::ignored, "main keyboard does not trigger");
    require(input.input(2, 30, true) == KeyboardFilter::pressed, "selected keyboard press");
    require(input.input(2, 30, true) == KeyboardFilter::ignored, "auto-repeat is ignored");
    require(input.input(1, 30, false) == KeyboardFilter::ignored, "main keyboard cannot release secondary key");
    require(input.input(2, 30, false) == KeyboardFilter::released, "selected keyboard release");
    require(input.input(2, 30, false) == KeyboardFilter::ignored, "unmatched release ignored");
    input.enable(false);
    require(input.input(2, 31, true) == KeyboardFilter::ignored, "disabled press tracked silently");
    input.enable(true);
    require(input.input(2, 31, true) == KeyboardFilter::ignored, "held key cannot restart on re-enable");
    require(input.input(2, 31, false) == KeyboardFilter::released, "held key release clears state");
    require(input.input(2, 31, true) == KeyboardFilter::pressed, "fresh key works after release");
    input.select(3);
    require(!input.isEnabled(), "changing device disarms capture");
    input.enable(true);
    require(input.input(2, 31, true) == KeyboardFilter::ignored, "old device cannot trigger after switch");
    require(input.input(3, 0x1d, true) == KeyboardFilter::pressed, "left modifier");
    require(input.input(3, 0x2001d, true) == KeyboardFilter::pressed, "extended right modifier independent");
    require(input.input(0, 30, true) == KeyboardFilter::ignored, "synthetic unknown source ignored");
    std::cout << "PASS: " << checks << " keyboard checks\n";
}

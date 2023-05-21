#include <test.hpp>

void spoilOnePackage(Package& p) {
    static bool spoiled = false;
    uint64_t id;
    memcpy(&id, p.id, sizeof(id));
    if (!spoiled && id == 2) {
        p.data[0] = p.data[1];
        spoiled = true;
    }
}

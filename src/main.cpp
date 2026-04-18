#include <iostream>
#include "storage/storage.h"

int main() {
    minikv::Storage db;

    db.set("name", "alice");

    auto val = db.get("name");
    if (val) {
        std::cout << "name: " << *val << std::endl;
    }

    db.del("name");

    if (!db.exists("name")) {
        std::cout << "deleted successfully" << std::endl;
    }

    std::cout << "size: " << db.size() << std::endl;

    return 0;
}
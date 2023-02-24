#include "VariadicTable.h"
#include <ryml.hpp>
#include "fmt/core.h"
#include <yaml-cpp/yaml.h>
#include <filesystem>

using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
void whereAmI() {
    for (const auto& dirEntry : recursive_directory_iterator("."))
        fmt::print("{}\n",dirEntry.path().string());
}


int main(int argc, char **argv) {

    whereAmI();

    // Faster
    char yml_buf[] = "{foo: 1, bar: [2, 3], bal: [4,5], john: doe}";
    ryml::Tree tree = ryml::parse_in_place(yml_buf);

    // Easier with more familiar c++ api and templating
    auto node = YAML::LoadFile("projects/examples/yamltable/data/data.yaml");

    VariadicTable<std::string, double, int, std::string> vt({"Key", "Value", "Age", "Brother"}, 10);

    vt.addRow(node["these"]["is"]["yaml"].as<std::string>(), node["alist"][0].as<int>(), 40, "John");
    vt.addRow(node["this"].as<std::string>(), node["alist"][1].as<int>(), 38, "Andrew");
    vt.addRow(node["of"].as<std::string>(), node["alist"][2].as<int>(), 27, "Fande");


    vt.print(std::cout);


    YAML::Node primes = YAML::Load("[2, 3, 5, 7, 11]");
    for (std::size_t i=0;i<primes.size();i++) {
        fmt::print("primes: {}\n",primes[i].as<int>());
    }


    fmt::print("testing this repo: {}\n", tree["foo"].is_keyval());
    return 0;
}
#include "patterns/manifest/ManifestWriter.hpp"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>

namespace patterns::manifest {

void ComponentManifestWriter::writeImpl(const std::filesystem::path& path,
                                         const std::string& component,
                                         const std::string& cppType,
                                         const std::string& version) const {
    std::filesystem::create_directories(path.parent_path());

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "component"       << YAML::Value << component;
    out << YAML::Key << "cpp_type"        << YAML::Value << cppType;
    out << YAML::Key << "current_version" << YAML::Value << version;
    out << YAML::Key << "versions"        << YAML::Value << YAML::BeginSeq;
    out << YAML::BeginMap;
    out << YAML::Key << "version" << YAML::Value << version;
    out << YAML::Key << "status"  << YAML::Value << "current";
    out << YAML::EndMap;
    out << YAML::EndSeq;
    out << YAML::EndMap;

    std::ofstream f(path);
    f << out.c_str() << "\n";
}

void ComponentManifestWriter::printManifestImpl(const std::filesystem::path& path) const {
    YAML::Node root = YAML::LoadFile(path.string());
    std::cout << "[Manifest] " << path.filename().string() << "\n"
              << "  component:       " << root["component"].as<std::string>()       << "\n"
              << "  cpp_type:        " << root["cpp_type"].as<std::string>()        << "\n"
              << "  current_version: " << root["current_version"].as<std::string>() << "\n";
}

} // namespace patterns::manifest

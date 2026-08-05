#pragma once
#include <filesystem>
#include <string>

namespace patterns::manifest {

// ─── Interface ────────────────────────────────────────────────────────────────

class IManifestWriter {
public:
    virtual ~IManifestWriter() = default;
    virtual void write(const std::filesystem::path& path,
                       const std::string& component,
                       const std::string& cppType,
                       const std::string& version) const = 0;
    virtual void printManifest(const std::filesystem::path& path) const = 0;
};

// ─── CRTP base ────────────────────────────────────────────────────────────────

template<typename Derived>
class ManifestWriterBase : public IManifestWriter {
public:
    void write(const std::filesystem::path& path,
               const std::string& component,
               const std::string& cppType,
               const std::string& version) const override {
        static_cast<const Derived*>(this)->writeImpl(path, component, cppType, version);
    }

    void printManifest(const std::filesystem::path& path) const override {
        static_cast<const Derived*>(this)->printManifestImpl(path);
    }
};

// ─── Concrete ─────────────────────────────────────────────────────────────────

class ComponentManifestWriter : public ManifestWriterBase<ComponentManifestWriter> {
public:
    void writeImpl(const std::filesystem::path& path,
                   const std::string& component,
                   const std::string& cppType,
                   const std::string& version) const;

    void printManifestImpl(const std::filesystem::path& path) const;
};

} // namespace patterns::manifest

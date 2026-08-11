#pragma once

#include "core/types.hpp"

#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace vidchopper {

enum class ConfigStore : u8 {
    Gui = 0,
    Cli = 1,
};

enum class ConfigMode : u8 {
    Native = 0,
    Portable = 1,
    Explicit = 2,
};

enum class ConfigPlatform : u8 {
    Windows = 0,
    MacOS = 1,
    Linux = 2,
};

struct ConfigPathOptions {
    std::optional<Path> explicit_path;
    bool portable {false};

    [[nodiscard]] auto operator==(const ConfigPathOptions&) const -> bool = default;
};

struct ConfigEnvironment {
    ConfigPlatform platform {
#if defined(_WIN32)
        ConfigPlatform::Windows
#elif defined(__APPLE__)
        ConfigPlatform::MacOS
#else
        ConfigPlatform::Linux
#endif
    };
    std::optional<Path> home_directory;
    std::optional<Path> xdg_config_home;
};

struct ConfigPaths {
    Path application_directory;
    Path config_root;
    Path settings_path;
    Path gui_settings_path;
    Path cli_settings_path;
    ConfigMode mode {ConfigMode::Native};

    [[nodiscard]] auto operator==(const ConfigPaths&) const -> bool = default;
};

struct ConfigResolutionResult {
    bool success {false};
    ConfigPaths paths;
    std::string error_message;

    [[nodiscard]] auto ok() const noexcept -> bool {
        return success;
    }
};

[[nodiscard]] inline auto config_file_name(const ConfigStore store) -> std::string_view {
    return store == ConfigStore::Gui ? std::string_view {"VidChopper.ini"} : std::string_view {"VidChopperCLI.ini"};
}

[[nodiscard]] inline auto config_current_directory() -> Path {
    auto error = std::error_code {};
    const Path path = std::filesystem::current_path(error);
    return error ? Path {"."} : path;
}

#ifdef _WIN32
[[nodiscard]] inline auto windows_environment_path(const wchar_t* name) -> std::optional<Path> {
    size_t required_size = 0;
    if (_wgetenv_s(&required_size, nullptr, 0, name) != 0 || required_size == 0) {
        return std::nullopt;
    }

    auto value = std::wstring(required_size, L'\0');
    size_t value_size = 0;
    if (_wgetenv_s(&value_size, value.data(), value.size(), name) != 0 || value_size == 0) {
        return std::nullopt;
    }
    value.resize(std::wcslen(value.c_str()));
    return value.empty() ? std::nullopt : std::optional<Path> {Path {std::move(value)}};
}
#endif

[[nodiscard]] inline auto environment_path(const char* name) -> std::optional<Path> {
#ifdef _WIN32
    char* value = nullptr;
    size_t value_size = 0;
    if (_dupenv_s(&value, &value_size, name) != 0 || value == nullptr || *value == '\0') {
        std::free(value);
        return std::nullopt;
    }
    auto result = std::optional<Path> {Path {std::string {value}}};
    std::free(value);
    return result;
#else
    const char* const value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return std::nullopt;
    }
    return Path {std::string {value}};
#endif
}

[[nodiscard]] inline auto current_config_environment() -> ConfigEnvironment {
    return ConfigEnvironment {
#if defined(_WIN32)
        .platform = ConfigPlatform::Windows,
        .home_directory = windows_environment_path(L"USERPROFILE"),
        .xdg_config_home = std::nullopt,
#elif defined(__APPLE__)
        .platform = ConfigPlatform::MacOS,
        .home_directory = environment_path("HOME"),
        .xdg_config_home = environment_path("XDG_CONFIG_HOME"),
#else
        .platform = ConfigPlatform::Linux,
        .home_directory = environment_path("HOME"),
        .xdg_config_home = environment_path("XDG_CONFIG_HOME"),
#endif
    };
}

[[nodiscard]] inline auto application_directory_for_config(const Path& executable_path) -> Path {
    if (executable_path.empty()) {
        return config_current_directory();
    }

    if (executable_path.has_parent_path()) {
        return executable_path.parent_path().lexically_normal();
    }

    return config_current_directory();
}

[[nodiscard]] inline auto macos_bundle_sidecar_directory(const Path& executable_path) -> Path {
    Path candidate = application_directory_for_config(executable_path);
    while (!candidate.empty()) {
        if (candidate.filename().extension() == ".app") {
            const Path outside_bundle = candidate.parent_path();
            return outside_bundle.empty() ? Path {"."} : outside_bundle;
        }

        const Path parent = candidate.parent_path();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }

    return application_directory_for_config(executable_path);
}

[[nodiscard]] inline auto native_config_root(const ConfigEnvironment& environment) -> std::optional<Path> {
    if (environment.platform == ConfigPlatform::Windows) {
        return std::nullopt;
    }

    if (!environment.home_directory.has_value() || environment.home_directory->empty()) {
        return std::nullopt;
    }

    if (environment.platform == ConfigPlatform::MacOS) {
        return *environment.home_directory / "Library" / "Application Support" / "VidChopper";
    }

    if (environment.xdg_config_home.has_value() && !environment.xdg_config_home->empty()) {
        if (!environment.xdg_config_home->is_absolute()) {
            return std::nullopt;
        }
        return *environment.xdg_config_home / "VidChopper";
    }

    return *environment.home_directory / ".config" / "VidChopper";
}

[[nodiscard]] inline auto resolve_config_paths(const Path& executable_path,
    const ConfigStore store,
    const ConfigPathOptions& options = {},
    const ConfigEnvironment& environment = current_config_environment()) -> ConfigResolutionResult {
    if (options.explicit_path.has_value() && options.portable) {
        return ConfigResolutionResult {
            .error_message = "Explicit config paths cannot be combined with portable mode.",
        };
    }

    const Path application_directory = application_directory_for_config(executable_path);
    auto mode = ConfigMode::Native;
    auto config_root = Path {};
    auto settings_path = Path {};

    if (options.explicit_path.has_value()) {
        mode = ConfigMode::Explicit;
        settings_path = options.explicit_path->lexically_normal();
        if (settings_path.empty() || settings_path.filename().empty()) {
            return ConfigResolutionResult {
                .error_message = "Explicit config path must name a file.",
            };
        }
        config_root = settings_path.has_parent_path() ? settings_path.parent_path() : config_current_directory();
    } else if (options.portable) {
        mode = ConfigMode::Portable;
        config_root = environment.platform == ConfigPlatform::MacOS ? macos_bundle_sidecar_directory(executable_path)
                                                                    : application_directory;
    } else if (environment.platform == ConfigPlatform::Windows) {
        config_root = application_directory;
    } else {
        const std::optional<Path> root = native_config_root(environment);
        if (!root.has_value()) {
            const std::string reason = environment.platform == ConfigPlatform::Linux
                    && environment.xdg_config_home.has_value() && !environment.xdg_config_home->is_absolute()
                ? "XDG_CONFIG_HOME must be an absolute path."
                : "HOME is required to resolve the native config directory.";
            return ConfigResolutionResult {.error_message = reason};
        }
        config_root = *root;
    }

    const Path gui_settings_path = config_root / std::string {config_file_name(ConfigStore::Gui)};
    const Path cli_settings_path = config_root / std::string {config_file_name(ConfigStore::Cli)};
    if (settings_path.empty()) {
        settings_path = store == ConfigStore::Gui ? gui_settings_path : cli_settings_path;
    }

    return ConfigResolutionResult {
        .success = true,
        .paths =
            ConfigPaths {
                .application_directory = application_directory,
                .config_root = config_root,
                .settings_path = settings_path,
                .gui_settings_path = gui_settings_path,
                .cli_settings_path = cli_settings_path,
                .mode = mode,
            },
    };
}

} // namespace vidchopper

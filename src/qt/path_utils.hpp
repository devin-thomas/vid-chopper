#pragma once

#include "core/path_utils.hpp"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <string>
#include <string_view>

namespace vidchopper {

[[nodiscard]] inline auto utf8_to_qstring(const std::string_view text) -> QString {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] inline auto qstring_to_utf8(const QString& value) -> std::string {
    const QByteArray encoded = value.toUtf8();
    return std::string {encoded.constData(), static_cast<size_t>(encoded.size())};
}

[[nodiscard]] inline auto path_to_qstring(const Path& path) -> QString {
    return utf8_to_qstring(path_to_utf8(path));
}

[[nodiscard]] inline auto qstring_to_path(const QString& value) -> Path {
    return path_from_utf8(qstring_to_utf8(value));
}

} // namespace vidchopper

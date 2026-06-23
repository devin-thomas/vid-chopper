from __future__ import annotations

from pathlib import Path
from textwrap import wrap

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
TMP_DIR = ROOT / "tmp" / "pdfs"
OUT_DIR = ROOT / "output" / "pdf"
PAGES_DIR = TMP_DIR / "cpp20_usage_report_pages"
PDF_PATH = OUT_DIR / "vidchopper-cpp20-usage-report.pdf"

WIDTH = 2550
HEIGHT = 3300
MARGIN = 110
CONTENT_X = MARGIN + 24
CONTENT_W = WIDTH - (MARGIN * 2) - 48

COL_GAP = 42
LEFT_COL_W = 1260
RIGHT_COL_W = CONTENT_W - LEFT_COL_W - COL_GAP

BG = "#F6F2EA"
INK = "#132238"
MUTED = "#5C6776"
ACCENT = "#146C94"
ACCENT_SOFT = "#D8EDF7"
BORDER = "#CCD6E1"
CARD = "#FFFDF9"
CODE_BG = "#0F1B2D"
CODE_FG = "#EDF5FF"
CODE_DIM = "#9CB2CC"
GREEN = "#3F8F6B"
AMBER = "#E3A73F"


def load_font(path_candidates: list[str], size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    for path in path_candidates:
        if Path(path).exists():
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


FONT_SANS = "C:/Windows/Fonts/segoeui.ttf"
FONT_SANS_BOLD = "C:/Windows/Fonts/segoeuib.ttf"
FONT_SERIF_BOLD = "C:/Windows/Fonts/georgiab.ttf"
FONT_CODE = "C:/Windows/Fonts/consola.ttf"

eyebrow_font = load_font([FONT_SANS_BOLD], 30)
title_font = load_font([FONT_SERIF_BOLD, FONT_SANS_BOLD], 74)
subtitle_font = load_font([FONT_SANS], 30)
card_title_font = load_font([FONT_SANS_BOLD], 28)
body_font = load_font([FONT_SANS], 24)
body_bold_font = load_font([FONT_SANS_BOLD], 24)
section_font = load_font([FONT_SANS_BOLD], 24)
small_font = load_font([FONT_SANS], 20)
small_bold_font = load_font([FONT_SANS_BOLD], 20)
pill_font = load_font([FONT_SANS_BOLD], 19)
code_label_font = load_font([FONT_SANS_BOLD], 19)
code_font = load_font([FONT_CODE], 21)
footer_font = load_font([FONT_SANS], 18)


def rgba(hex_color: str) -> tuple[int, int, int]:
    hex_color = hex_color.lstrip("#")
    return tuple(int(hex_color[i : i + 2], 16) for i in (0, 2, 4))


def text_size(draw: ImageDraw.ImageDraw, text: str, font: ImageFont.ImageFont) -> tuple[int, int]:
    left, top, right, bottom = draw.textbbox((0, 0), text, font=font)
    return right - left, bottom - top


def wrap_paragraph(draw: ImageDraw.ImageDraw, text: str, font: ImageFont.ImageFont, max_width: int) -> list[str]:
    words = text.split()
    lines: list[str] = []
    current = ""
    for word in words:
        candidate = word if not current else f"{current} {word}"
        if text_size(draw, candidate, font)[0] <= max_width:
            current = candidate
        else:
            if current:
                lines.append(current)
            current = word
    if current:
        lines.append(current)
    return lines


def draw_paragraph(
    draw: ImageDraw.ImageDraw,
    x: int,
    y: int,
    width: int,
    text: str,
    font: ImageFont.ImageFont = body_font,
    fill: str = INK,
    line_gap: int = 8,
) -> int:
    lines = wrap_paragraph(draw, text, font, width)
    _, line_h = text_size(draw, "Ag", font)
    for line in lines:
        draw.text((x, y), line, font=font, fill=fill)
        y += line_h + line_gap
    return y


def draw_bullets(draw: ImageDraw.ImageDraw, x: int, y: int, width: int, items: list[str]) -> int:
    bullet_w = 24
    for item in items:
        draw.rounded_rectangle((x, y + 10, x + 8, y + 18), radius=4, fill=ACCENT)
        y = draw_paragraph(draw, x + bullet_w, y, width - bullet_w, item, font=body_font, fill=INK, line_gap=8)
        y += 10
    return y


def draw_pills(draw: ImageDraw.ImageDraw, x: int, y: int, labels: list[str]) -> int:
    cur_x = x
    cur_y = y
    pad_x = 18
    pad_y = 10
    gap = 12
    for label in labels:
        w, h = text_size(draw, label, pill_font)
        box = (cur_x, cur_y, cur_x + w + pad_x * 2, cur_y + h + pad_y * 2)
        if box[2] > x + 860:
            cur_x = x
            cur_y += h + pad_y * 2 + 10
            box = (cur_x, cur_y, cur_x + w + pad_x * 2, cur_y + h + pad_y * 2)
        draw.rounded_rectangle(box, radius=24, fill=ACCENT_SOFT)
        draw.text((box[0] + pad_x, box[1] + pad_y - 2), label, font=pill_font, fill=ACCENT)
        cur_x = box[2] + gap
    return cur_y + h + pad_y * 2


def draw_card(
    draw: ImageDraw.ImageDraw,
    x: int,
    y: int,
    w: int,
    h: int,
    title: str,
    text: str,
    pills: list[str] | None = None,
) -> None:
    draw.rounded_rectangle((x, y, x + w, y + h), radius=28, fill=CARD, outline=BORDER, width=3)
    draw.text((x + 26, y + 22), title, font=card_title_font, fill=INK)
    cur_y = y + 62
    cur_y = draw_paragraph(draw, x + 26, cur_y, w - 52, text, font=body_font, fill=INK, line_gap=7)
    if pills:
        draw_pills(draw, x + 26, cur_y + 18, pills)


def draw_code_block(
    draw: ImageDraw.ImageDraw,
    x: int,
    y: int,
    w: int,
    label: str,
    code: str,
) -> int:
    lines = code.splitlines()
    _, code_h = text_size(draw, "Ag", code_font)
    label_h = text_size(draw, label, code_label_font)[1]
    line_gap = 8
    height = 26 + label_h + 18 + len(lines) * (code_h + line_gap) + 26
    draw.rounded_rectangle((x, y, x + w, y + height), radius=26, fill=CODE_BG)
    draw.text((x + 24, y + 20), label.upper(), font=code_label_font, fill=CODE_DIM)
    cur_y = y + 20 + label_h + 18
    for line in lines:
        draw.text((x + 24, cur_y), line, font=code_font, fill=CODE_FG)
        cur_y += code_h + line_gap
    return y + height


def new_page() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGB", (WIDTH, HEIGHT), rgba(BG))
    draw = ImageDraw.Draw(image)

    draw.rounded_rectangle(
        (MARGIN, MARGIN, WIDTH - MARGIN, HEIGHT - MARGIN),
        radius=44,
        fill=rgba("#FFFDFC"),
        outline=rgba(BORDER),
        width=3,
    )
    draw.rounded_rectangle(
        (MARGIN, MARGIN, WIDTH - MARGIN, MARGIN + 18),
        radius=10,
        fill=rgba(ACCENT),
    )
    draw.rectangle((MARGIN + 760, MARGIN, MARGIN + 1515, MARGIN + 18), fill=rgba(GREEN))
    draw.rectangle((MARGIN + 1515, MARGIN, WIDTH - MARGIN, MARGIN + 18), fill=rgba(AMBER))
    return image, draw


def draw_header(draw: ImageDraw.ImageDraw, title: str, subtitle: str) -> int:
    y = MARGIN + 58
    draw.text((CONTENT_X, y), "VIDCHOPPER C++20 AUDIT", font=eyebrow_font, fill=ACCENT)
    y += 58
    draw.text((CONTENT_X, y), title, font=title_font, fill=INK)
    y += 112
    y = draw_paragraph(draw, CONTENT_X, y, CONTENT_W, subtitle, font=subtitle_font, fill=MUTED, line_gap=10)
    return y


PAGE_DEFS = [
    {
        "title": "Feature 1: Designated Initializers",
        "subtitle": (
            "This is the main reason the project requires C++20. The build enforces "
            "CMAKE_CXX_STANDARD 20 and target_compile_features(vidchopper_core PUBLIC cxx_std_20), "
            "and designated initializers appear across the core library, Qt services, UI model, and tests."
        ),
        "cards": [
            (
                0.58,
                360,
                "Where it appears",
                "Eight files use C++20 aggregate designators: src/core/chapter_plan.cpp, "
                "src/core/command_builder.cpp, src/qt/services/export_coordinator.cpp, "
                "src/qt/services/ffprobe_service.cpp, src/qt/ui/chapter_table_model.cpp, and three tests. "
                "These sites construct data-model structs such as ChapterSegment, ResolvedEncoder, "
                "VideoProbeResult, and PendingExport.",
                None,
            ),
            (
                0.42,
                360,
                "Why it matters here",
                "VidChopper is structured around small, plain structs with defaulted members. Named field "
                "initialization lets each call site set only the fields it cares about while inheriting defaults "
                "for the rest, without depending on declaration order.",
                ["Core library", "Qt service layer", "Tests mirror production data"],
            ),
        ],
        "left_title": "In-context C++20 Example",
        "left_blocks": [
            (
                "src/core/command_builder.cpp:49-68",
                """if (settings.encoder_kind == EncoderKind::HevcNvenc) {
    return ResolvedEncoder {
        .kind = EncoderKind::HevcNvenc,
        .video_codec = "hevc_nvenc",
        .arguments = {"-preset", settings.nvenc_preset, "-cq",
            std::to_string(settings.nvenc_cq), "-rc", "vbr_hq"},
    };
}

return ResolvedEncoder {
    .kind = EncoderKind::X264,
    .video_codec = "libx264",
    .arguments = {"-preset", settings.x264_preset, "-crf",
        std::to_string(settings.x264_crf)},
};""",
            ),
            (
                "More production uses",
                """src/core/chapter_plan.cpp:32-36
chapters.push_back(ChapterSegment {
    .name = "Chapter " + std::to_string(index + 1),
    .start_ms = start_ms,
    .end_ms = end_ms,
});

src/qt/services/ffprobe_service.cpp:129-132
return VideoProbeResult {
    .success = metadata.duration_ms > 0,
    .error_message = metadata.duration_ms > 0 ? QString {} : ...,
    .metadata = std::move(metadata),
};""",
            ),
        ],
        "right_title": "C++17 Equivalent",
        "right_code": (
            "Same intent without C++20 designators",
            """ResolvedEncoder resolved {};
resolved.kind = EncoderKind::HevcNvenc;
resolved.video_codec = "hevc_nvenc";
resolved.arguments = {
    "-preset", settings.nvenc_preset,
    "-cq", std::to_string(settings.nvenc_cq),
    "-rc", "vbr_hq"
};
return resolved;""",
        ),
        "right_sections": [
            (
                "Why this version was chosen",
                [
                    "It makes object construction self-documenting at the call site.",
                    "It avoids brittle positional aggregate initialization for larger structs.",
                    "It lets omitted members keep their struct defaults cleanly.",
                    "It keeps tests and production code using the same data-construction style.",
                ],
            ),
            (
                "Difference it makes",
                [
                    "Performance is effectively neutral versus a default-construct-then-assign sequence.",
                    "The codebase benefit is large: many model-construction sites become shorter and safer to edit.",
                    "Field-order mistakes are easier to spot because every populated member is named explicitly.",
                ],
            ),
        ],
        "footer_left": "Evidence base: exhaustive scan of src/, tests/, and build files.",
    },
    {
        "title": "Feature 2: Ranges Algorithm Overload",
        "subtitle": (
            "The core library uses std::ranges::transform in the path-normalization logic that selects a safe output "
            "extension. This is a real C++20 dependency in shipped code, not just a test convenience."
        ),
        "cards": [
            (
                0.5,
                310,
                "Where it appears",
                "One occurrence in src/core/command_builder.cpp inside safe_source_extension. The function lowercases "
                "the extension in place before deciding whether the source container is safe to preserve.",
                None,
            ),
            (
                0.5,
                310,
                "Why it matters here",
                "The code works on a whole string object, not on a pair of iterators stored elsewhere. The ranges "
                "overload expresses that directly and keeps the transformation local to the range being mutated.",
                None,
            ),
        ],
        "left_title": "In-context C++20 Example",
        "left_blocks": [
            (
                "src/core/command_builder.cpp:10-27",
                """auto safe_source_extension(const VideoMetadata& metadata)
    -> std::string {
    auto extension = metadata.source_extension;
    if (extension.empty()) {
        extension = metadata.source_path.extension().string();
    }

    if (extension.empty()) {
        return ".mp4";
    }

    std::ranges::transform(extension, extension.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });

    if (extension != ".mp4" && extension != ".mkv" &&
        extension != ".mov") {
        return ".mp4";
    }
    return extension;
}""",
            )
        ],
        "right_title": "C++17 Equivalent",
        "right_code": (
            "Using iterator pairs",
            """std::transform(
    extension.begin(),
    extension.end(),
    extension.begin(),
    [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });""",
        ),
        "right_sections": [
            (
                "Why this version was chosen",
                [
                    "The algorithm takes the range object directly, so the code repeats less iterator boilerplate.",
                    "It reads like 'transform this string' instead of 'transform begin/end into begin'.",
                    "It matches the rest of the codebase's operate-on-whole-objects style.",
                ],
            ),
            (
                "Difference it makes",
                [
                    "Runtime behavior is essentially the same as std::transform: in-place lowercase conversion, no extra allocation.",
                    "The gain is mainly maintainability and intent clarity, not raw speed.",
                    "Even though this is only one site, it still makes the core target require a C++20 library implementation.",
                ],
            ),
        ],
        "footer_left": "This feature contributes to the C++20 requirement even when the GUI and tests are disabled.",
    },
     {
        "title": "Feature 3: C++20 String Suffix API",
        "subtitle": (
            "The test suite uses the C++20 convenience member ends_with to assert suffix behavior in command generation. "
            "This is not the main reason the product needs C++20, but it is one of the project's actual post-17 features."
        ),
        "cards": [
            (
                0.5,
                315,
                "Where it appears",
                "One occurrence in tests/test_command_builder.cpp. The test verifies that source container mode preserves "
                "a supported source extension rather than forcing .mp4.",
                None,
            ),
            (
                0.5,
                315,
                "Scope note",
                "This feature is test-only. If it were the only C++20 usage, the application itself could still be "
                "lowered to C++17 with test rewrites. The production code cannot, because designated initializers and "
                "ranges are used in shipped targets.",
                None,
            ),
        ],
        "left_title": "In-context C++20 Example",
        "left_blocks": [
            (
                "tests/test_command_builder.cpp:36-43",
                """const auto command = build_ffmpeg_command(
    metadata, chapter, output_path, settings,
    EncoderEnvironment {});

test_support::expect_true(
    command.back().ends_with(".mov"),
    "source container mode should preserve common source extension"
);""",
            )
        ],
        "right_title": "C++17 Equivalent",
        "right_code": (
            "Manual suffix check",
            """const auto& output = command.back();
const auto has_mov_suffix =
    output.size() >= 4 &&
    output.compare(output.size() - 4, 4, ".mov") == 0;

test_support::expect_true(
    has_mov_suffix,
    "source container mode should preserve common source extension"
);""",
        ),
        "right_sections": [
            (
                "Why this version was chosen",
                [
                    "It states the intent of the assertion directly.",
                    "It avoids repeated boundary arithmetic in test code.",
                    "It keeps the test focused on behavior instead of string-index bookkeeping.",
                ],
            ),
            (
                "Difference it makes",
                [
                    "Performance difference is negligible and irrelevant in a unit test.",
                    "The real benefit is correctness and readability: no risk of forgetting the length guard or getting the offset wrong.",
                    "Because the use is isolated to tests, its impact on the overall codebase is small compared with designated initializers.",
                ],
            ),
            (
                "Exclusions from the audit",
                [
                    "The scan deliberately excluded framework APIs that look similar, such as QString::contains and QString::startsWith.",
                    "Those are Qt features, not reasons to require C++20.",
                ],
            ),
        ],
        "footer_left": "Post-17 feature set found in this repository: designated initializers, ranges, and ends_with.",
    },
]


def render_page(page_index: int, page_def: dict) -> Image.Image:
    image, draw = new_page()
    y = draw_header(draw, page_def["title"], page_def["subtitle"])
    y += 34

    card_gap = 28
    card_x = CONTENT_X
    for width_ratio, height, title, text, pills in page_def["cards"]:
        card_w = int((CONTENT_W - card_gap) * width_ratio)
        draw_card(draw, card_x, y, card_w, height, title, text, pills)
        card_x += card_w + card_gap

    y += max(card[1] for card in page_def["cards"]) + 46

    left_x = CONTENT_X
    right_x = CONTENT_X + LEFT_COL_W + COL_GAP

    draw.text((left_x, y), page_def["left_title"].upper(), font=section_font, fill=ACCENT)
    draw.text((right_x, y), page_def["right_title"].upper(), font=section_font, fill=ACCENT)
    left_y = y + 40
    right_y = y + 40

    for label, code in page_def["left_blocks"]:
        left_y = draw_code_block(draw, left_x, left_y, LEFT_COL_W, label, code) + 22

    right_y = draw_code_block(draw, right_x, right_y, RIGHT_COL_W, page_def["right_code"][0], page_def["right_code"][1]) + 24

    for section_title, bullets in page_def["right_sections"]:
        draw.text((right_x, right_y), section_title.upper(), font=section_font, fill=ACCENT)
        right_y += 38
        right_y = draw_bullets(draw, right_x, right_y, RIGHT_COL_W, bullets) + 16

    footer_y = HEIGHT - MARGIN - 44
    draw.text((CONTENT_X, footer_y), page_def["footer_left"], font=footer_font, fill=MUTED)
    footer_text = f"Page {page_index + 1} of {len(PAGE_DEFS)}"
    footer_w, _ = text_size(draw, footer_text, footer_font)
    draw.text((WIDTH - MARGIN - 24 - footer_w, footer_y), footer_text, font=footer_font, fill=MUTED)

    return image


def main() -> None:
    TMP_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    PAGES_DIR.mkdir(parents=True, exist_ok=True)

    pages = []
    for index, page_def in enumerate(PAGE_DEFS):
        image = render_page(index, page_def)
        page_path = PAGES_DIR / f"page_{index + 1}.png"
        image.save(page_path, format="PNG")
        pages.append(image.convert("RGB"))

    first, rest = pages[0], pages[1:]
    first.save(PDF_PATH, "PDF", resolution=300.0, save_all=True, append_images=rest)
    print(PDF_PATH)


if __name__ == "__main__":
    main()

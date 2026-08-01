#!/usr/bin/env python3
"""Render a bounded Markdown subset as a deterministic architecture PDF.

Reusable interface:
  python docs/scripts/render-guide-pdf.py \
    --input docs/<guide>.md --output docs/<guide>.pdf \
    --label "VIDCHOPPER ARCHITECTURE" --subtitle "Manager and developer reference"

The Codex bundled document runtime includes ReportLab. Other environments may install
ReportLab 4.4+ without changing the generated artifact contract.
"""

from __future__ import annotations

import argparse
import hashlib
import html
import re
from pathlib import Path
from typing import Iterable

try:
    from reportlab import rl_config
    from reportlab.lib import colors
    from reportlab.lib.enums import TA_LEFT
    from reportlab.lib.pagesizes import LETTER
    from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
    from reportlab.lib.units import inch
    from reportlab.platypus import (
        BaseDocTemplate,
        Frame,
        HRFlowable,
        ListFlowable,
        ListItem,
        LongTable,
        PageBreak,
        Paragraph,
        PageTemplate,
        Spacer,
        Table,
        TableStyle,
    )
except ModuleNotFoundError as error:
    raise SystemExit(
        "ReportLab 4.4+ is required. Use the bundled Codex document runtime or "
        "install it with: python -m pip install 'reportlab>=4.4,<5'"
    ) from error


PAGE_WIDTH, PAGE_HEIGHT = LETTER
CONTENT_WIDTH = PAGE_WIDTH - 1.08 * inch
NAVY = colors.HexColor("#15324A")
BLUE = colors.HexColor("#1E6A8D")
PALE_BLUE = colors.HexColor("#EAF3F7")
INK = colors.HexColor("#1F2933")
MUTED = colors.HexColor("#5C6975")
RULE = colors.HexColor("#B9C8D2")
PAPER = colors.HexColor("#F8FAFB")


def normalized_source(text: str) -> str:
    return text.replace("\r\n", "\n").replace("\r", "\n").rstrip() + "\n"


def source_digest(text: str) -> str:
    return hashlib.sha256(normalized_source(text).encode("utf-8")).hexdigest()


def inline_markup(text: str) -> str:
    value = html.escape(text.strip())
    value = re.sub(
        r"\[([^\]]+)\]\(([^)]+)\)",
        lambda match: (
            f'<font color="#1E6A8D"><u>{match.group(1)}</u></font>'
            f' <font size="7.5" color="#5C6975">({match.group(2)})</font>'
        ),
        value,
    )
    value = re.sub(r"`([^`]+)`", r'<font name="Courier">\1</font>', value)
    value = re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", value)
    value = re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", r"<i>\1</i>", value)
    return value


def split_table_row(line: str) -> list[str]:
    return [cell.strip() for cell in line.strip().strip("|").split("|")]


def is_table_separator(line: str) -> bool:
    cells = split_table_row(line)
    return bool(cells) and all(re.fullmatch(r":?-{3,}:?", cell) for cell in cells)


def is_block_start(lines: list[str], index: int) -> bool:
    line = lines[index]
    stripped = line.strip()
    if not stripped:
        return True
    if re.match(r"^#{1,4}\s+", line):
        return True
    if stripped.startswith(("~~~", "```")):
        return True
    if stripped == "<!-- pagebreak -->":
        return True
    if re.match(r"^\s*[-*]\s+", line) or re.match(r"^\s*\d+\.\s+", line):
        return True
    if stripped.startswith(">") or stripped in {"---", "***"}:
        return True
    return (
        stripped.startswith("|")
        and index + 1 < len(lines)
        and is_table_separator(lines[index + 1])
    )


def table_widths(column_count: int) -> list[float]:
    weights = {
        2: [0.30, 0.70],
        3: [0.22, 0.30, 0.48],
        4: [0.16, 0.24, 0.30, 0.30],
    }.get(column_count, [1.0 / column_count] * column_count)
    return [CONTENT_WIDTH * weight for weight in weights]


def make_styles() -> dict[str, ParagraphStyle]:
    sample = getSampleStyleSheet()
    return {
        "body": ParagraphStyle(
            "Body",
            parent=sample["BodyText"],
            fontName="Helvetica",
            fontSize=8.6,
            leading=11.2,
            textColor=INK,
            spaceAfter=5,
        ),
        "quote": ParagraphStyle(
            "Quote",
            parent=sample["BodyText"],
            fontName="Helvetica",
            fontSize=8.2,
            leading=10.5,
            textColor=NAVY,
            leftIndent=10,
            borderColor=BLUE,
            borderWidth=1.5,
            borderPadding=7,
            backColor=PALE_BLUE,
            spaceAfter=8,
        ),
        "h2": ParagraphStyle(
            "H2",
            parent=sample["Heading2"],
            fontName="Helvetica-Bold",
            fontSize=15,
            leading=18,
            textColor=NAVY,
            spaceBefore=12,
            spaceAfter=5,
            keepWithNext=True,
        ),
        "h3": ParagraphStyle(
            "H3",
            parent=sample["Heading3"],
            fontName="Helvetica-Bold",
            fontSize=10.5,
            leading=13,
            textColor=BLUE,
            spaceBefore=8,
            spaceAfter=4,
            keepWithNext=True,
        ),
        "h4": ParagraphStyle(
            "H4",
            parent=sample["Heading4"],
            fontName="Helvetica-Bold",
            fontSize=9,
            leading=11,
            textColor=INK,
            spaceBefore=7,
            spaceAfter=3,
            keepWithNext=True,
        ),
        "table": ParagraphStyle(
            "TableCell",
            parent=sample["BodyText"],
            fontName="Helvetica",
            fontSize=7.2,
            leading=9,
            textColor=INK,
            spaceAfter=0,
        ),
        "table_header": ParagraphStyle(
            "TableHeader",
            parent=sample["BodyText"],
            fontName="Helvetica-Bold",
            fontSize=7.2,
            leading=9,
            textColor=colors.white,
            spaceAfter=0,
        ),
        "code": ParagraphStyle(
            "Code",
            parent=sample["Code"],
            fontName="Courier",
            fontSize=6.3,
            leading=7.8,
            textColor=INK,
            leftIndent=0,
            rightIndent=0,
            wordWrap="CJK",
        ),
        "mast_label": ParagraphStyle(
            "MastLabel",
            parent=sample["BodyText"],
            fontName="Helvetica-Bold",
            fontSize=8,
            leading=10,
            textColor=colors.HexColor("#BDE6F3"),
            spaceAfter=8,
        ),
        "mast_title": ParagraphStyle(
            "MastTitle",
            parent=sample["Title"],
            fontName="Helvetica-Bold",
            fontSize=25,
            leading=28,
            textColor=colors.white,
            alignment=TA_LEFT,
            spaceAfter=8,
        ),
        "mast_subtitle": ParagraphStyle(
            "MastSubtitle",
            parent=sample["BodyText"],
            fontName="Helvetica",
            fontSize=9.5,
            leading=12,
            textColor=colors.white,
        ),
    }


def code_flowable(code: str, styles: dict[str, ParagraphStyle]) -> Table:
    lines = []
    for raw_line in code.splitlines() or [""]:
        escaped = html.escape(raw_line).replace(" ", "&#160;")
        lines.append(escaped or "&#160;")
    paragraph = Paragraph("<br/>".join(lines), styles["code"])
    table = Table([[paragraph]], colWidths=[CONTENT_WIDTH], hAlign="LEFT")
    table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), PAPER),
                ("BOX", (0, 0), (-1, -1), 0.5, RULE),
                ("LEFTPADDING", (0, 0), (-1, -1), 7),
                ("RIGHTPADDING", (0, 0), (-1, -1), 7),
                ("TOPPADDING", (0, 0), (-1, -1), 6),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
            ]
        )
    )
    return table


def markdown_story(
    source: str,
    label: str,
    subtitle: str,
    digest: str,
) -> tuple[str, list[object]]:
    lines = normalized_source(source).splitlines()
    styles = make_styles()
    title = "Architecture Guide"
    for line in lines:
        match = re.match(r"^#\s+(.+)$", line)
        if match:
            title = match.group(1).strip()
            break

    masthead = Table(
        [[
            [
                Paragraph(html.escape(label.upper()), styles["mast_label"]),
                Paragraph(html.escape(title), styles["mast_title"]),
                Paragraph(html.escape(subtitle), styles["mast_subtitle"]),
                Spacer(1, 7),
                Paragraph(
                    f'<font size="7.5" color="#BDE6F3">'
                    f'Source SHA-256 {digest}</font>',
                    styles["mast_subtitle"],
                ),
            ]
        ]],
        colWidths=[CONTENT_WIDTH],
        hAlign="LEFT",
    )
    masthead.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), NAVY),
                ("LEFTPADDING", (0, 0), (-1, -1), 22),
                ("RIGHTPADDING", (0, 0), (-1, -1), 22),
                ("TOPPADDING", (0, 0), (-1, -1), 18),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 18),
            ]
        )
    )
    story: list[object] = [masthead, Spacer(1, 12)]
    index = 0
    skipped_title = False

    while index < len(lines):
        line = lines[index]
        stripped = line.strip()
        if not stripped:
            index += 1
            continue

        if stripped == "<!-- pagebreak -->":
            story.append(PageBreak())
            index += 1
            continue

        heading = re.match(r"^(#{1,4})\s+(.+)$", line)
        if heading:
            depth = len(heading.group(1))
            text = heading.group(2).strip()
            index += 1
            if depth == 1 and not skipped_title:
                skipped_title = True
                continue
            style = styles[{2: "h2", 3: "h3", 4: "h4"}.get(depth, "h2")]
            story.append(Paragraph(inline_markup(text), style))
            continue

        if stripped.startswith(("~~~", "```")):
            fence = stripped[:3]
            index += 1
            code_lines = []
            while index < len(lines) and not lines[index].strip().startswith(fence):
                code_lines.append(lines[index])
                index += 1
            index += 1
            story.extend([code_flowable("\n".join(code_lines), styles), Spacer(1, 7)])
            continue

        if (
            stripped.startswith("|")
            and index + 1 < len(lines)
            and is_table_separator(lines[index + 1])
        ):
            rows = [split_table_row(line)]
            index += 2
            while index < len(lines) and lines[index].strip().startswith("|"):
                rows.append(split_table_row(lines[index]))
                index += 1
            column_count = len(rows[0])
            data = []
            for row_index, row in enumerate(rows):
                padded = (row + [""] * column_count)[:column_count]
                style = styles["table_header" if row_index == 0 else "table"]
                data.append([Paragraph(inline_markup(cell), style) for cell in padded])
            table = LongTable(
                data,
                colWidths=table_widths(column_count),
                repeatRows=1,
                hAlign="LEFT",
                splitByRow=1,
            )
            table.setStyle(
                TableStyle(
                    [
                        ("BACKGROUND", (0, 0), (-1, 0), NAVY),
                        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, PAPER]),
                        ("GRID", (0, 0), (-1, -1), 0.35, RULE),
                        ("VALIGN", (0, 0), (-1, -1), "TOP"),
                        ("LEFTPADDING", (0, 0), (-1, -1), 4),
                        ("RIGHTPADDING", (0, 0), (-1, -1), 4),
                        ("TOPPADDING", (0, 0), (-1, -1), 4),
                        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
                    ]
                )
            )
            story.extend([table, Spacer(1, 7)])
            continue

        list_match = re.match(r"^\s*([-*]|\d+\.)\s+(.+)$", line)
        if list_match:
            ordered = list_match.group(1)[0].isdigit()
            items = []
            while index < len(lines):
                item_match = re.match(r"^\s*([-*]|\d+\.)\s+(.+)$", lines[index])
                if not item_match or item_match.group(1)[0].isdigit() != ordered:
                    break
                items.append(
                    ListItem(
                        Paragraph(inline_markup(item_match.group(2)), styles["body"]),
                        leftIndent=12,
                    )
                )
                index += 1
            story.append(
                ListFlowable(
                    items,
                    bulletType="1" if ordered else "bullet",
                    start="1" if ordered else "-",
                    leftIndent=16,
                    bulletFontName="Helvetica",
                    bulletFontSize=7.5,
                    bulletColor=BLUE,
                    spaceAfter=5,
                )
            )
            continue

        if stripped.startswith(">"):
            quote_lines = []
            while index < len(lines) and lines[index].strip().startswith(">"):
                quote_lines.append(lines[index].strip()[1:].strip())
                index += 1
            story.append(Paragraph(inline_markup(" ".join(quote_lines)), styles["quote"]))
            continue

        if stripped in {"---", "***"}:
            story.extend([HRFlowable(width="100%", thickness=0.6, color=RULE), Spacer(1, 5)])
            index += 1
            continue

        paragraph_lines = [stripped]
        index += 1
        while index < len(lines) and not is_block_start(lines, index):
            paragraph_lines.append(lines[index].strip())
            index += 1
        story.append(Paragraph(inline_markup(" ".join(paragraph_lines)), styles["body"]))

    return title, story


def render(
    input_path: Path,
    output_path: Path,
    label: str,
    subtitle: str,
) -> tuple[str, int]:
    rl_config.invariant = True
    source = input_path.read_text(encoding="utf-8")
    digest = source_digest(source)
    title, story = markdown_story(source, label, subtitle, digest)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    document = BaseDocTemplate(
        str(output_path),
        pagesize=LETTER,
        rightMargin=0.54 * inch,
        leftMargin=0.54 * inch,
        topMargin=0.62 * inch,
        bottomMargin=0.48 * inch,
        title=title,
        author="VidChopper contributors",
        subject=f"Source-SHA256:{digest}",
        creator="VidChopper deterministic architecture renderer",
        producer="ReportLab",
        pageCompression=1,
        invariant=1,
    )

    page_count = 0

    def decorate_page(canvas, doc) -> None:
        nonlocal page_count
        page_count = max(page_count, doc.page)
        canvas.saveState()
        canvas.setStrokeColor(RULE)
        canvas.setLineWidth(0.45)
        canvas.line(0.54 * inch, PAGE_HEIGHT - 0.38 * inch, PAGE_WIDTH - 0.54 * inch, PAGE_HEIGHT - 0.38 * inch)
        canvas.setFont("Helvetica-Bold", 6.8)
        canvas.setFillColor(NAVY)
        canvas.drawString(0.54 * inch, PAGE_HEIGHT - 0.29 * inch, label.upper())
        canvas.setFont("Helvetica", 6.5)
        canvas.setFillColor(MUTED)
        canvas.drawRightString(PAGE_WIDTH - 0.54 * inch, PAGE_HEIGHT - 0.29 * inch, title)
        canvas.line(0.54 * inch, 0.33 * inch, PAGE_WIDTH - 0.54 * inch, 0.33 * inch)
        canvas.drawString(0.54 * inch, 0.21 * inch, f"Source {digest[:12]}")
        canvas.drawRightString(PAGE_WIDTH - 0.54 * inch, 0.21 * inch, f"Page {doc.page}")
        canvas.restoreState()

    frame = Frame(
        document.leftMargin,
        document.bottomMargin,
        document.width,
        document.height,
        id="guide-content",
        leftPadding=0,
        rightPadding=0,
        topPadding=0,
        bottomPadding=0,
    )
    document.addPageTemplates(
        [PageTemplate(id="guide-page", frames=[frame], onPage=decorate_page)]
    )
    document.build(story)
    return digest, page_count


def parse_args(arguments: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="Authoritative Markdown source")
    parser.add_argument("--output", required=True, type=Path, help="Tracked PDF destination")
    parser.add_argument("--label", default="VIDCHOPPER ARCHITECTURE")
    parser.add_argument("--subtitle", default="Manager and developer reference")
    return parser.parse_args(arguments)


def main() -> int:
    arguments = parse_args()
    digest, page_count = render(
        arguments.input,
        arguments.output,
        arguments.label,
        arguments.subtitle,
    )
    print(
        f"Rendered {arguments.output} ({page_count} pages, "
        f"source SHA-256 {digest})."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

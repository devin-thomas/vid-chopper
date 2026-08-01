# VidChopper Domain Context

This glossary is the canonical vocabulary for VidChopper product and architecture discussions. The
definitions describe the domain independently of any user interface, command-line syntax, library,
storage layout, or code structure.

## Source

One input video.

## Chapter

An ordered intent containing a name, start, and end. A Chapter describes a requested portion of a
Source, not an output artifact.

## RenderedSegment

The produced media artifact for a Chapter. The word "clip" is reserved for user-facing copy; domain
documentation and architecture discussions use RenderedSegment.

## ChapterFile

A JSON or YAML serialization of Chapters and optional overrides.

## ChapterSource

A ChapterFile or explicit embedded chapters.

## Job

One Source plus its resolved ChapterSource and plan, effective settings, planned paths, and
RenderedSegments.

## Batch

Multiple Jobs.

## Architecture Decisions

Durable rationale for the system boundaries and support promise lives in the
[architecture decision record index](knowledge/architecture/decisions/README.md).
